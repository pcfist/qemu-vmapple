/*
 * crc16.h
 * ICC protocol
 *
 * Copyright (C) 2017 Alexander Graf <agraf@suse.de>
 * SPDX-License-Identifier: GPL-2.0
 */

uint16_t crc16ccitt_xmodem(uint8_t *message, int nBytes);
