#!/usr/bin/env python
#
#  Virtio Ring Analyzer
#
#  Copyright (c) 2018 Alexander Graf <agraf@suse.de>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.

import numpy as np
import json
import os
import argparse
import collections
import pprint

class VirtIOFile:
    def read64(self):
        return np.asscalar(np.fromfile(self.file, count=1, dtype='<i8')[0]) & 0xffffffffffffffff

    def read32(self):
        return np.asscalar(np.fromfile(self.file, count=1, dtype='<i4')[0]) & 0xffffffff

    def read16(self):
        return np.asscalar(np.fromfile(self.file, count=1, dtype='<i2')[0]) & 0xffff

    def read8(self):
        return np.asscalar(np.fromfile(self.file, count=1, dtype='<i1')[0]) & 0xff

    def __init__(self, filename, offset):
        self.file = open(filename, "rb")
        self.file.seek(offset, os.SEEK_SET)

class VirtQueueElement:

    def __init__(self, file):
        self.file = file
        self.data = collections.OrderedDict()

    def read(self):
        self.data["addr"] = hex(self.file.read64())
        self.data["len"] = hex(self.file.read32())
        self.data["flags"] = hex(self.file.read16())
        self.data["next"] = hex(self.file.read16())

        return self.data
        
    def __repr__(self):
        return self.data.__repr__()

    def __str__(self):
        return self.data.__str__()

class VirtQueue:
    def __init__(self, filename, offset, num):
        self.file = VirtIOFile(filename, offset)
        self.num = num
        self.data = collections.OrderedDict()
        self.data['buffers'] = collections.OrderedDict()
        self.data['available'] = collections.OrderedDict()
        self.data['used'] = collections.OrderedDict()

        for i in xrange(0, num):
            self.data['buffers'][i] = VirtQueueElement(self.file).read()

        self.data['available']['flags'] = hex(self.file.read16())
        self.data['available']['index'] = hex(self.file.read16())
        ring = [ self.file.read16() for i in xrange(0, num) ]
        self.data['available']['ring'] = " ".join("{0:02x}".format(c) for c in ring)
        self.data['available']['irq_index'] = hex(self.file.read16())

        pos = self.file.file.tell()
        print "old pos: %x" % pos
        self.file.file.seek((pos + 4095) & ~4095)
        print "new pos: %x" % self.file.file.tell()

        self.data['used']['flags'] = hex(self.file.read16())
        self.data['used']['index'] = hex(self.file.read16())
        self.data['used']['ring'] = collections.OrderedDict()
        ring = self.data['used']['ring']
        for i in xrange(0, num):
            ring[i] = collections.OrderedDict()
            ring[i]['index'] = hex(self.file.read32())
            ring[i]['len'] = hex(self.file.read32())
        self.data['used']['irq_index'] = hex(self.file.read16())

    def __repr__(self):
        return self.data.__repr__()

    def __str__(self):
        return self.data.__str__()

class JSONEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, VirtQueue) or isinstance(o, VirtQueueElement):
            return o.data
        raise TypeError

parser = argparse.ArgumentParser()
parser.add_argument("-m", "--memory", help='Memory dump to read from', required=True)
parser.add_argument("-o", "--offset", help='Offset of the virtqueue', required=True, type=int)
parser.add_argument("-n", "--num",    help='Number of queue elements', required=True, type=int)
args = parser.parse_args()

jsonenc = JSONEncoder(indent=4, separators=(',', ': '))

vq = VirtQueue(args.memory, args.offset, args.num)
print jsonenc.encode(vq)
