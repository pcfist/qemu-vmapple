softmmu_ss.add(when: 'CONFIG_VMAPPLE_AES',  if_true: files('aes.c'))
softmmu_ss.add(when: 'CONFIG_VMAPPLE_BDIF', if_true: files('bdif.c'))
softmmu_ss.add(when: 'CONFIG_VMAPPLE_CFG',  if_true: files('cfg.c'))
softmmu_ss.add(when: 'CONFIG_VMAPPLE_PVG',  if_true: [files('apple-gfx.m'), pvg, metal])
