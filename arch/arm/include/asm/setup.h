/*
 *  linux/include/asm/setup.h
 *
 *  Copyright (C) 1997-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Structure passed to kernel to tell it about the
 *  hardware it's running on.  See Documentation/arm/Setup
 *  for more info.
 */
#ifndef __ASMARM_SETUP_H
#define __ASMARM_SETUP_H

#include <linux/types.h>

#define COMMAND_LINE_SIZE 1024

/* The list ends with an ATAG_NONE node. */
#define ATAG_NONE	0x00000000

struct tag_header {
	__u32 size;
	__u32 tag;
};

/* The list must start with an ATAG_CORE node */
#define ATAG_CORE	0x54410001

struct tag_core {
	__u32 flags;		/* bit 0 = read-only */
	__u32 pagesize;
	__u32 rootdev;
};

/* it is allowed to have multiple ATAG_MEM nodes */
#define ATAG_MEM	0x54410002

struct tag_mem32 {
	__u32	size;
	__u32	start;	/* physical start address */
};

/* VGA text type displays */
#define ATAG_VIDEOTEXT	0x54410003

struct tag_videotext {
	__u8		x;
	__u8		y;
	__u16		video_page;
	__u8		video_mode;
	__u8		video_cols;
	__u16		video_ega_bx;
	__u8		video_lines;
	__u8		video_isvga;
	__u16		video_points;
};

/* describes how the ramdisk will be used in kernel */
#define ATAG_RAMDISK	0x54410004

struct tag_ramdisk {
	__u32 flags;	/* bit 0 = load, bit 1 = prompt */
	__u32 size;	/* decompressed ramdisk size in _kilo_ bytes */
	__u32 start;	/* starting block of floppy-based RAM disk image */
};

/* describes where the compressed ramdisk image lives (virtual address) */
/*
 * this one accidentally used virtual addresses - as such,
 * it's deprecated.
 */
#define ATAG_INITRD	0x54410005

/* describes where the compressed ramdisk image lives (physical address) */
#define ATAG_INITRD2	0x54420005

struct tag_initrd {
	__u32 start;	/* physical start address */
	__u32 size;	/* size of compressed ramdisk image in bytes */
};

/* board serial number. "64 bits should be enough for everybody" */
#define ATAG_SERIAL	0x54410006

struct tag_serialnr {
	__u32 low;
	__u32 high;
};

/* board revision */
#define ATAG_REVISION	0x54410007

struct tag_revision {
	__u32 rev;
};

/* initial values for vesafb-type framebuffers. see struct screen_info
 * in include/linux/tty.h
 */
#define ATAG_VIDEOLFB	0x54410008

struct tag_videolfb {
	__u16		lfb_width;
	__u16		lfb_height;
	__u16		lfb_depth;
	__u16		lfb_linelength;
	__u32		lfb_base;
	__u32		lfb_size;
	__u8		red_size;
	__u8		red_pos;
	__u8		green_size;
	__u8		green_pos;
	__u8		blue_size;
	__u8		blue_pos;
	__u8		rsvd_size;
	__u8		rsvd_pos;
};

/* command line: \0 terminated string */
#define ATAG_CMDLINE	0x54410009

struct tag_cmdline {
	char	cmdline[1];	/* this is the minimum size */
};

#if defined(CONFIG_LAB126)
/* 16 byte id for serial number. "64-bits wasn't enough for us." */
#define ATAG_SERIAL16   0x5441000a

/* 16 byte id for a board revision. */
#define ATAG_REVISION16 0x5441000b

/* 16 digit alphanumeric id used for serial numbers, board ids, etc. */
struct tag_id16 {
	__u8 data[16];
};
/* 32 bit value for POST results. */
#define ATAG_POST	0x5441000c

struct tag_post {
	__u32 failure;
};

/* mac address / secret */
#define ATAG_MACADDR	0x5441000d

struct tag_macaddr {
   u8 address[12];
   u8 secret[20];
};

/* bootmode/postmode variables */
#define ATAG_BOOTMODE	0x5441000f

struct tag_bootmode {
   u8 boot[16];
   u8 post[16];
};

/* LPDDR2 vendor ID */
#define ATAG_DDRMFGID   0x54410010

struct tag_ddrmfgid {
   __u32 mfgid;
};

#endif

/* acorn RiscPC specific information */
#define ATAG_ACORN	0x41000101

struct tag_acorn {
	__u32 memc_control_reg;
	__u32 vram_pages;
	__u8 sounddefault;
	__u8 adfsdrives;
};

/* footbridge memory clock, see arch/arm/mach-footbridge/arch.c */
#define ATAG_MEMCLK	0x41000402

struct tag_memclk {
	__u32 fmemclk;
};

struct tag {
	struct tag_header hdr;
	union {
		struct tag_core		core;
		struct tag_mem32	mem;
		struct tag_videotext	videotext;
		struct tag_ramdisk	ramdisk;
		struct tag_initrd	initrd;
		struct tag_serialnr	serialnr;
		struct tag_revision	revision;
		struct tag_videolfb	videolfb;
		struct tag_cmdline	cmdline;
#if defined(CONFIG_LAB126)
		struct tag_id16 id16;
		struct tag_post post;
		struct tag_macaddr macaddr;
		struct tag_bootmode bootmode;
		struct tag_ddrmfgid ddrmfgid;
#endif
		/*
		 * Acorn specific
		 */
		struct tag_acorn	acorn;

		/*
		 * DC21285 specific
		 */
		struct tag_memclk	memclk;
	} u;
};

struct tagtable {
	__u32 tag;
	int (*parse)(const struct tag *);
};

#define tag_member_present(tag,member)				\
	((unsigned long)(&((struct tag *)0L)->member + 1)	\
		<= (tag)->hdr.size * 4)

#define tag_next(t)	((struct tag *)((__u32 *)(t) + (t)->hdr.size))
#define tag_size(type)	((sizeof(struct tag_header) + sizeof(struct type)) >> 2)

#define for_each_tag(t,base)		\
	for (t = base; t->hdr.size; t = tag_next(t))

#ifdef __KERNEL__

#define __tag __used __attribute__((__section__(".taglist.init")))
#define __tagtable(tag, fn) \
static struct tagtable __tagtable_##fn __tag = { tag, fn }

/*
 * Memory map description
 */
#define NR_BANKS 8

struct membank {
	phys_addr_t start;
	unsigned long size;
	unsigned int highmem;
};

struct meminfo {
	int nr_banks;
	struct membank bank[NR_BANKS];
};

extern struct meminfo meminfo;

#define for_each_bank(iter,mi)				\
	for (iter = 0; iter < (mi)->nr_banks; iter++)

#define bank_pfn_start(bank)	__phys_to_pfn((bank)->start)
#define bank_pfn_end(bank)	__phys_to_pfn((bank)->start + (bank)->size)
#define bank_pfn_size(bank)	((bank)->size >> PAGE_SHIFT)
#define bank_phys_start(bank)	(bank)->start
#define bank_phys_end(bank)	((bank)->start + (bank)->size)
#define bank_phys_size(bank)	(bank)->size

extern int arm_add_memory(phys_addr_t start, unsigned long size);
extern void early_print(const char *str, ...);
extern void dump_machine_table(void);

#endif  /*  __KERNEL__  */

#endif
