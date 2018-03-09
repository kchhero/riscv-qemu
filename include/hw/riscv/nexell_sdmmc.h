/*
 * Raspberry Pi (NEXELL) SD Host Controller
 *
 * Copyright (c) 2017 Antfield SAS
 *
 * Authors:
 *  Clement Deschamps <clement.deschamps@antfield.fr>
 *  Luc Michel <luc.michel@antfield.fr>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef NEXELL_SDMMC_H
#define NEXELL_SDMMC_H

#include "hw/sysbus.h"
#include "hw/sd/sd.h"

#define TYPE_NEXELL_SDMMC "riscv.nexell.sdmmc"
#define NEXELL_SDMMC(obj) \
        OBJECT_CHECK(NexellSDMMCState, (obj), TYPE_NEXELL_SDMMC)

#define NEXELL_SDMMC_FIFO_LEN 16
#define NEXELL_SWALLOW_NUM_SDMMC 3

typedef struct NexellSDMMCState{
	volatile unsigned int CTRL;			// 0x000 : Control
	volatile unsigned int PWREN;			// 0x004 : Power Enable
	volatile unsigned int CLKDIV;			// 0x008 : Clock Divider
	volatile unsigned int CLKSRC;			// 0x00C : Clock Source
	volatile unsigned int CLKENA;			// 0x010 : Clock Enable
	volatile unsigned int TMOUT;			// 0x014 : Time-Out
	volatile unsigned int CTYPE;			// 0x018 : Card Type
	volatile unsigned int BLKSIZ;			// 0x01C : Block Size
	volatile unsigned int BYTCNT;			// 0x020 : Byte Count
	volatile unsigned int INTMASK;			// 0x024 : Interrupt Mask
	volatile unsigned int CMDARG;			// 0x028 : Command Argument
	volatile unsigned int CMD;			// 0x02C : Command
	volatile unsigned int RESP[4];			// 0x030 : Response 0
	volatile unsigned int MINTSTS;			// 0x040 : Masked Interrupt Status
	volatile unsigned int RINTSTS;			// 0x044 : Raw Interrupt Status
	volatile unsigned int STATUS;			// 0x048 : Status - Mainly for Debug Purpose
	volatile unsigned int FIFOTH;			// 0x04C : FIFO Threshold
	volatile unsigned int CDETECT;			// 0x050 : Card Detect
	volatile unsigned int WRTPRT;			// 0x054 : Write Protect
	volatile unsigned int GPIO;			// 0x058 : General Purpose Input/Output
	volatile unsigned int TCBCNT;			// 0x05C : Transferred CIU card byte count
	volatile unsigned int TBBCNT;			// 0x060 : Transferred Host Byte Count
	volatile unsigned int DEBNCE;			// 0x064 : Card Detect Debounce
	volatile unsigned int USRID;			// 0x068 : User ID
	volatile unsigned int VERID;			// 0x06C : Version ID
	volatile unsigned int HCON;			// 0x070 : Hardware Configuration
	volatile unsigned int UHS_REG;			// 0x074 : UHS_REG register
	volatile unsigned int RSTn;			// 0x078 : Hardware reset register
	volatile unsigned int _Rev0;			// 0x07C
	volatile unsigned int BMOD;			// 0x080 : Bus Mode
	volatile unsigned int PLDMND;			// 0x084 : Poll Demand
	volatile unsigned int DBADDR;			// 0x088 : Descriptor List Base Address
	volatile unsigned int IDSTS;			// 0x08C : Internal DMAC Status
	volatile unsigned int IDINTEN;			// 0x090 : Internal DMAC Interrupt Enable
	volatile unsigned int DSCADDR;			// 0x094 : Current Host Descriptor Address
	volatile unsigned int BUFADDR;			// 0x098 : Current Buffer Descriptor Address
	volatile unsigned char  _Rev1[0x100-0x09C];	// 0x09C ~ 0x100 reserved area
	volatile unsigned int CARDTHRCTL;		// 0x100 : Card Read Threshold Enable
	volatile unsigned int BACKEND_POWER;		// 0x104 : Back-end Power
	volatile unsigned int UHS_REG_EXT;		// 0x108 : eMMC 4.5 1.2V
	volatile unsigned int EMMC_DDR_REG;		// 0x10C : eMMC DDR START bit detection control
	volatile unsigned int ENABLE_SHIFT;		// 0x110 : Phase shift control
	volatile unsigned int CLKCTRL;			// 0x114 : External clock phase & delay control
	volatile unsigned char  _Rev2[0x200-0x118];	// 0x118 ~ 0x200
	volatile unsigned int DATA;			// 0x200 : Data
	volatile unsigned char  _Rev3[0x400-0x204];	// 0x204 ~ 0x400
	volatile unsigned int TIEMODE;			// 0x400
	volatile unsigned int TIESRAM;			// 0x404
	volatile unsigned int TIEDRVPHASE;		// 0x408
	volatile unsigned int TIESMPPHASE;		// 0x40C
	volatile unsigned int TIEDSDELAY;		// 0x410
        SysBusDevice busdev;
        SDBus sdbus;
        MemoryRegion mmio;

        qemu_irq irq;
} NexellSDMMCState;


#define    NEXELL_SDMMC__CTRL           0x000 // Control
#define    NEXELL_SDMMC__PWREN  	 0x004 // Power Enable
#define    NEXELL_SDMMC__CLKDIV	 0x008 // Clock Divider
#define    NEXELL_SDMMC__CLKSRC	 0x00C // Clock Source
#define    NEXELL_SDMMC__CLKENA	 0x010 // Clock Enable
#define    NEXELL_SDMMC__TMOUT	         0x014 // Time-Out
#define    NEXELL_SDMMC__CTYPE	         0x018 // Card Type
#define    NEXELL_SDMMC__BLKSIZ	 0x01C // Block Size
#define    NEXELL_SDMMC__BYTCNT	 0x020 // Byte Count
#define    NEXELL_SDMMC__INTMASK	 0x024 // Interrupt Mask
#define    NEXELL_SDMMC__CMDARG	 0x028 // Command Argument
#define    NEXELL_SDMMC__CMD		 0x02C // Command
#define    NEXELL_SDMMC__RESP   	 0x030 // Response 0
#define    NEXELL_SDMMC__MINTSTS	 0x040 // Masked Interrupt Status
#define    NEXELL_SDMMC__RINTSTS	 0x044 // Raw Interrupt Status
#define    NEXELL_SDMMC__STATUS	 0x048 // Status - Mainly for Debug Purpose
#define    NEXELL_SDMMC__FIFOTH	 0x04C // FIFO Threshold
#define    NEXELL_SDMMC__CDETECT	 0x050 // Card Detect
#define    NEXELL_SDMMC__WRTPRT	 0x054 // Write Protect
#define    NEXELL_SDMMC__GPIO		 0x058 // General Purpose Input/Output
#define    NEXELL_SDMMC__TCBCNT	 0x05C // Transferred CIU card byte count
#define    NEXELL_SDMMC__TBBCNT	 0x060 // Transferred Host Byte Count
#define    NEXELL_SDMMC__DEBNCE	 0x064 // Card Detect Debounce
#define    NEXELL_SDMMC__USRID 	 0x068 // User ID
#define    NEXELL_SDMMC__VERID	         0x06C // Version ID
#define    NEXELL_SDMMC__HCON		 0x070 // Hardware Configuration
#define    NEXELL_SDMMC__UHS_REG	 0x074 // UHS_REG register
#define    NEXELL_SDMMC__RSTn		 0x078 // Hardware reset register
#define    NEXELL_SDMMC___Rev0 	 0x07C
#define    NEXELL_SDMMC__BMOD		 0x080 // Bus Mode
#define    NEXELL_SDMMC__PLDMND	 0x084 // Poll Demand
#define    NEXELL_SDMMC__DBADDR	 0x088 // Descriptor List Base Address
#define    NEXELL_SDMMC__IDSTS 	 0x08C // Internal DMAC Status
#define    NEXELL_SDMMC__IDINTEN	 0x090 // Internal DMAC Interrupt Enable
#define    NEXELL_SDMMC__DSCADDR	 0x094 // Current Host Descriptor Address
#define    NEXELL_SDMMC__BUFADDR	 0x098 // Current Buffer Descriptor Address
#define    NEXELL_SDMMC___Rev1          0x09C // [0x100-0x09C];	0x09C ~ 0x100 reserved area
#define    NEXELL_SDMMC__CARDTHRCTL		0x100 // Card Read Threshold Enable
#define    NEXELL_SDMMC__BACKEND_POWER 	0x104 // Back-end Power
#define    NEXELL_SDMMC__UHS_REG_EXT		0x108 // eMMC 4.5 1.2V
#define    NEXELL_SDMMC__EMMC_DDR_REG		0x10C // eMMC DDR START bit detection control
#define    NEXELL_SDMMC__ENABLE_SHIFT		0x110 // Phase shift control
#define    NEXELL_SDMMC__CLKCTRL		0x114 // External clock phase & delay control
#define    NEXELL_SDMMC___Rev2                 0x118 //[0x200-0x118];	0x118 ~ 0x200
#define    NEXELL_SDMMC__DATA			0x200 // Data
#define    NEXELL_SDMMC___Rev3                 0x204 //[0x400-0x204];	0x204 ~ 0x400
#define    NEXELL_SDMMC__TIEMODE		0x400
#define    NEXELL_SDMMC__TIESRAM		0x404
#define    NEXELL_SDMMC__TIEDRVPHASE		0x408
#define    NEXELL_SDMMC__TIESMPPHASE		0x40C
#define    NEXELL_SDMMC__TIEDSDELAY		0x410

NexellSDMMCState *nexell_sdmmc_create(MemoryRegion *address_space, hwaddr base, qemu_irq irq);

#endif
