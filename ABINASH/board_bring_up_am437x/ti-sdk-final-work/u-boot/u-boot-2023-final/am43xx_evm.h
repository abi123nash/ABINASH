/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * am43xx_evm.h
 *
 * Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
 */

#ifndef __CONFIG_AM43XX_EVM_H
#define __CONFIG_AM43XX_EVM_H

#define CFG_MAX_RAM_BANK_SIZE	(1024 << 21)	/* 2GB */
#define CFG_SYS_TIMERBASE		0x48040000	/* Use Timer2 */

#include <asm/arch/omap.h>

#define __SD_BOOT__
/* NS16550 Configuration */
#define CFG_SYS_NS16550_CLK		48000000

/* Enabling L2 Cache */
#define CFG_SYS_PL310_BASE	0x48242000

/*
 * When building U-Boot such that there is no previous loader
 * we need to call board_early_init_f.  This is taken care of in
 * s_init when we have SPL used.
 */

/* Now bring in the rest of the common code. */
#include <configs/ti_armv7_omap.h>

/* Clock Defines */
#define V_OSCK				24000000  /* Clock output from T2 */
#define V_SCLK				(V_OSCK)

/* NS16550 Configuration */
#define CFG_SYS_NS16550_COM1		0x44e09000	/* Base EVM has UART0 */

#ifndef CONFIG_SPL_BUILD
/* USB Device Firmware Update support */
#define DFUARGS \
	"dfu_bufsiz=0x10000\0" \
	DFU_ALT_INFO_MMC \
	DFU_ALT_INFO_EMMC \
	DFU_ALT_INFO_RAM \
	DFU_ALT_INFO_QSPI_XIP
#else
#define DFUARGS
#endif

#define BOOTENV_DEV_NAND(devtypeu, devtypel, instance) \
	"bootcmd_" #devtypel #instance "=" \
	"run nandboot\0"

#define BOOTENV_DEV_NAME_NAND(devtypeu, devtypel, instance) \
	#devtypel #instance " "

#if 0
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(USB, usb, 0) \
	func(NAND, nand, 0) \
	func(PXE, pxe, na) \
	func(DHCP, dhcp, na)
#else
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(NAND, nand, 0) 
#endif
#include <config_distro_bootcmd.h>

#ifndef CONFIG_SPL_BUILD
#include <environment/ti/dfu.h>

#define CFG_EXTRA_ENV_SETTINGS \
	DEFAULT_LINUX_BOOT_ENV \
	DEFAULT_FIT_TI_ARGS \
	"fdtfile=undefined\0" \
	"finduuid=part uuid mmc 0:2 uuid\0" \
	"console=ttyS0,115200n8\0" \
	"partitions=" \
		"uuid_disk=${uuid_gpt_disk};" \
		"name=rootfs,start=2MiB,size=-,uuid=${uuid_gpt_rootfs}\0" \
	"optargs=\0" \
	"ramroot=/dev/ram0 rw\0" \
	"ramrootfstype=ext2\0" \
	"ramargs=setenv bootargs console=${console} " \
		"${optargs} " \
		"root=${ramroot} " \
		"rootfstype=${ramrootfstype}\0" \
	"loadramdisk=load ${devtype} ${devnum} ${rdaddr} ramdisk.gz\0" \
	"findfdt="\
		"if test $board_name = AM43EPOS; then " \
			"setenv fdtfile am43x-epos-evm.dtb; fi; " \
		"if test $board_name = AM43__GP; then " \
			"setenv fdtfile am437x-gp-evm.dtb; fi; " \
		"if test $board_name = AM43XXHS; then " \
			"setenv fdtfile am437x-gp-evm.dtb; fi; " \
		"if test $board_name = AM43__SK; then " \
			"setenv fdtfile am437x-sk-evm.dtb; fi; " \
		"if test $board_name = AM43_IDK; then " \
			"setenv fdtfile am437x-idk-evm.dtb; fi; " \
		"if test $fdtfile = undefined; then " \
			"echo WARNING: Could not determine device tree; fi; \0" \
	NANDARGS \
	NETARGS \
	DFUARGS \
	BOOTENV

#endif

#define PHY_ANEG_TIMEOUT	8000 /* PHY needs longer aneg time at 1G */

/* NAND support */
#ifdef CONFIG_MTD_RAW_NAND
/* NAND: device related configs */
/* NAND: driver related configs */
#define CFG_SYS_NAND_ECCPOS	{ 2, 3, 4, 5, 6, 7, 8, 9, \
				10, 11, 12, 13, 14, 15, 16, 17, 18, 19, \
				20, 21, 22, 23, 24, 25, 26, 27, 28, 29, \
				30, 31, 32, 33, 34, 35, 36, 37, 38, 39, \
				40, 41, 42, 43, 44, 45, 46, 47, 48, 49, \
				50, 51, 52, 53, 54, 55, 56, 57}
       



#define CFG_SYS_NAND_ECCSIZE		512
#define CFG_SYS_NAND_ECCBYTES	14

#ifdef __SD_BOOT__
#define NANDARGS \
	"nandargs=setenv bootargs console=${console} " \
		"${optargs} " \
		"root=${nandroot} " \
		"rootfstype=${nandrootfstype}\0" \
	"nandroot=/dev/mmcblk0p2 rw; \0" \
	"nandrootfstype=ext4 rootwait=10; \0" \
	"nandboot=echo Booting from MMC...; " \
		"run nandargs; " \
                "fatload mmc 0:1 ${addr_fit} fitImage.itb; " \
                "bootm ${addr_fit} \0"
#else
#define NANDARGS \
	"nandargs=setenv bootargs console=${console} " \
		"${optargs} " \
		"root=${nandroot} " \
		"rootfstype=${nandrootfstype}\0" \
	"nandroot=ubi0:rootfs rw ubi.mtd=NAND.file-system,2048\0" \
	"nandrootfstype=ubifs rootwait\0" \
	"nandboot=echo Booting from nand...; " \
		"run nandargs; " \
		"nand read ${fdtaddr} NAND.u-boot-spl-os; " \
		"nand read ${addr_fit} NAND.kernel; " \
		"bootm ${addr_fit} \0"
#endif

#define NANDBOOT			"run nandboot; "
#else /* !CONFIG_MTD_RAW_NAND */
#define NANDARGS
#define NANDBOOT
#endif /* CONFIG_MTD_RAW_NAND */

#if defined(CONFIG_TI_SECURE_DEVICE)
/* Avoid relocating onto firewalled area at end of DRAM */
#define CFG_PRAM (64 * 1024)
#endif /* CONFIG_TI_SECURE_DEVICE */

#endif	/* __CONFIG_AM43XX_EVM_H */
