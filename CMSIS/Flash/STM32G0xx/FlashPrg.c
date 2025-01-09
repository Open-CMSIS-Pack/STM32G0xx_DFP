/* -----------------------------------------------------------------------------
 * Copyright (c) 2014 - 2021 ARM Ltd.
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software. Permission is granted to anyone to use this
 * software for any purpose, including commercial applications, and to alter
 * it and redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software in
 *    a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 *
 *
 * $Date:        10. December 2021
 * $Revision:    V1.2.0
 *
 * Project:      Flash Programming Functions for ST STM32G0xx Flash
 * --------------------------------------------------------------------------- */

/* History:
 *  Version 1.2.0
 *    Added DualBank support & Rework
 *  Version 1.1.0
 *    Added DB OPT algorithms
 *  Version 1.0.0
 *    Initial release
 */

/* Note: Flash Page numbering
  Single Bank Devices:
    0x0800 0000 - 0x0801 FFFF      bank number: 0,  page number 0..63
  256kB Dual Bank Devices configured as ‘Dual Bank’ (DUAL_BANK = 1):
    0x0800 0000 - 0x0801 FFFF      bank number: 0,  page number 0..63
    0x0802 0000 - 0x0803 FFFF      bank number: 1,  page number 0..63
  256kB Dual Bank Devices configured as ‘Single Bank’ (DUAL_BANK = 0):
    0x0800 0000 - 0x0801 FFFF      bank number: 0,  page number 0..63
    0x0802 0000 - 0x0803 FFFF      bank number: 0,  page number 64..127
  512kB Dual Bank Devices configured as ‘Dual Bank’ (DUAL_BANK = 1):
    0x0800 0000 - 0x0803 FFFF      bank number: 0,  page number 0..127
    0x0804 0000 - 0x0807 FFFF      bank number: 1,  page number 0..127
  512kB Dual Bank Devices configured as ‘Single Bank’ (DUAL_BANK = 0):
    0x0800 0000 - 0x0803 FFFF      bank number: 0,  page number 0..127
    0x0804 0000 - 0x0807 FFFF      bank number: 1,  page number 0..127
  512kB devices are always handled as ‘Dual Bank’ even if they are configured as ‘Single Bank’.
 */

#include "..\FlashOS.h"        /* FlashOS Structures */

typedef volatile unsigned long    vu32;
typedef          unsigned long     u32;

#define M32(adr) (*((vu32 *) (adr)))

/* Peripheral Memory Map */
#define WWDG_BASE         (0x40002C00U)
#define IWDG_BASE         (0x40003000U)
#define FLASH_BASE        (0x40022000U)
#define DBGMCU_BASE       (0x40015800U)
#define FLASHSIZE_BASE    (0x1FFF75E0U)

#define WWDG            ((WWDG_TypeDef   *) WWDG_BASE)
#define IWDG            ((IWDG_TypeDef   *) IWDG_BASE)
#define FLASH           ((FLASH_TypeDef  *) FLASH_BASE)
#define DBGMCU          ((DBGMCU_TypeDef *) DBGMCU_BASE)

/* Debug MCU */
typedef struct {
  vu32 IDCODE;
} DBGMCU_TypeDef;

/* Independent WATCHDOG */
typedef struct {
  vu32 KR;               /* Offset: 0x00 Key Register */
  vu32 PR;               /* Offset: 0x04 Prescaler Register */
  vu32 RLR;              /* Offset: 0x08 Reload Register */
  vu32 SR;               /* Offset: 0x0C Status Register */
  vu32 WINR;             /* Offset: 0x10 Window Register */
} IWDG_TypeDef;

/* Flash Registers */
typedef struct {
  vu32 ACR;              /* Offset: 0x00  Access Control Register */
  vu32 RESERVED0;        /* Offset: 0x04  Reserved */
  vu32 KEYR;             /* Offset: 0x08  Key Register */
  vu32 OPTKEYR;          /* Offset: 0x0C  Option Key Register */
  vu32 SR;               /* Offset: 0x10  Status Register */
  vu32 CR;               /* Offset: 0x14  Control Register */
  vu32 ECCR;             /* Offset: 0x18  ECC Register */
  vu32 RESERVED1;        /* Offset: 0x1C  Reserved */
  vu32 OPTR;             /* Offset: 0x20  Option Register */
  vu32 PCROP1ASR;        /* Offset: 0x24  Bank PCROP1 Area A Start address Register */
  vu32 PCROP1AER;        /* Offset: 0x28  Bank PCROP1 Area A End address Register */
  vu32 WRP1AR;           /* Offset: 0x2C  Bank WRP1 Area A Address Register */
  vu32 WRP1BR;           /* Offset: 0x30  Bank WRP1 Area B Address Register */
  vu32 PCROP1BSR;        /* Offset: 0x34  Bank PCROP1 Area B Start Address Register */
  vu32 PCROP1BER;        /* Offset: 0x38  Bank PCROP1 Area B End Address Register */
  vu32 RESERVED2[2];
  vu32 PCROP2ASR;        /* Offset: 0x44  Bank PCROP2 Area A Start Address Register */
  vu32 PCROP2AER;        /* Offset: 0x48  Bank PCROP2 Area A End Address Register */
  vu32 WRP2AR;           /* Offset: 0x4C  Bank WRP2 Area A Address Register */
  vu32 WRP2BR;           /* Offset: 0x50  Bank WRP2 Area B Address Register */
  vu32 PCROP2BSR;        /* Offset: 0x54  Bank PCROP1 Area B Start Address Register */
  vu32 PCROP2BER;        /* Offset: 0x58  Bank PCROP1 Area B End Address Register */
  vu32 RESERVED3[9];
  vu32 SECR;             /* Offset: 0x80  Security Register */
} FLASH_TypeDef;


/* WWDG Registers */
typedef struct {
  vu32 CR;               /* Offset: 0x00 Control Register */
  vu32 CFR;              /* Offset: 0x04 Configuration Register */
  vu32 SR;               /* Offset: 0x08 Status Register */
} WWDG_TypeDef;


/* Flash Keys */
#define FLASH_KEY1               0x45670123
#define FLASH_KEY2               0xCDEF89AB
#define FLASH_OPTKEY1            0x08192A3B
#define FLASH_OPTKEY2            0x4C5D6E7F

/* Flash Access Control Register definitions */
#define FLASH_ACR_EMPTY         ((u32)(   1U << 16))

/* Flash Control Register definitions */
#define FLASH_CR_PG             ((u32)(   1U      ))
#define FLASH_CR_PER            ((u32)(   1U <<  1))
#define FLASH_CR_MER1           ((u32)(   1U <<  2))
#define FLASH_CR_PNB_MSK        ((u32)(0x7FU <<  3))
#define FLASH_CR_MER2           ((u32)(   1U << 15))
#define FLASH_CR_STRT           ((u32)(   1U << 16))
#define FLASH_CR_OPTSTRT        ((u32)(   1U << 17))
#define FLASH_CR_OBL_LAUNCH     ((u32)(   1U << 27))
#define FLASH_CR_OPTLOCK        ((u32)(   1U << 30))
#define FLASH_CR_LOCK           ((u32)(   1U << 31))

/* Flash Status Register definitions */
#define FLASH_SR_EOP            ((u32)(   1U      ))
#define FLASH_SR_OPERR          ((u32)(   1U <<  1))
#define FLASH_SR_PROGERR        ((u32)(   1U <<  3))
#define FLASH_SR_WRPERR         ((u32)(   1U <<  4))
#define FLASH_SR_PGAERR         ((u32)(   1U <<  5))
#define FLASH_SR_SIZERR         ((u32)(   1U <<  6))
#define FLASH_SR_PGSERR         ((u32)(   1U <<  7))
#define FLASH_SR_MISSERR        ((u32)(   1U <<  8))
#define FLASH_SR_FASTERR        ((u32)(   1U <<  9))
#define FLASH_SR_RDERR          ((u32)(   1U << 14))
#define FLASH_SR_OPTVERR        ((u32)(   1U << 15))
#define FLASH_SR_BSY1           ((u32)(   1U << 16))
#define FLASH_SR_BSY2           ((u32)(   1U << 17))

#define FLASH_SR_BSY            (FLASH_SR_BSY1 | FLASH_SR_BSY2)
#define FLASH_PGERR             (FLASH_SR_OPERR   | FLASH_SR_PROGERR | FLASH_SR_WRPERR | \
                                 FLASH_SR_PGAERR  | FLASH_SR_SIZERR  | FLASH_SR_PGSERR | \
                                 FLASH_SR_MISSERR | FLASH_SR_FASTERR | FLASH_SR_RDERR  | FLASH_SR_OPTVERR )


/* Flash option register definitions */
#define FLASH_OPTR_RDP          ((u32)(0xFFU      ))
#define FLASH_OPTR_RDP_NO       ((u32)(0xAAU      ))
#define FLASH_OPTR_IDWG_SW      ((u32)(   1U << 16))
#define FLASH_OPTR_WWDG_SW      ((u32)(   1U << 19))
#define FLASH_OPTR_DBANK        ((u32)(   1U << 21))


u32 flashType;                   /* Flash type, single/dual bank */
u32 flashBase;                   /* Flash base address */
u32 flashSize;                   /* Flash size in bytes */
u32 flashBankSize;               /* Flash bank size in bytes */
u32 flashBankMode;               /* Flash bank mode, configured as single or dual bank */

static void __NOP(void) {
    __asm("NOP");
}

static void __DSB(void) {
    __asm("DSB");
}


/*
 * Get Flash Type
 *    Return Value:   0 = Single-Bank Flash
 *                    1 = Dual-Bank Flash (configurable)
 */

#if defined FLASH_MEM
static u32 GetFlashType (void) {
  u32 flashType;

  switch ((DBGMCU->IDCODE & 0xFFFU)) {
    case 0x456:             /* STM32G050xx, STM32G051xx, STM32G061xx */
    case 0x460:             /* STM32G070xx, STM32G071xx, STM32G081xx */
    case 0x466:             /* STM32G030xx */
                            /* devices have only a singe bank flash */
      flashType = 0U;       /* Single-Bank Flash type */
    break;
 
    case 0x467:             /* STM32G0B0xx, STM32G0B1xx, STM32G0C1xx */
    default:                /* devices have a dual bank flash, configurable? */
      flashType = 1U;       /* Dual-Bank Flash type */
    break;
  }

  return (flashType);
}
#endif /* FLASH_MEM */


/*
 * Get Flash Bank Mode
 *    Return Value:   0 = Single-Bank mode
 *                    1 = Dual-Bank mode
 */

#if defined FLASH_MEM
static u32 GetFlashBankMode (void) {
  u32 flashBankMode;

  flashBankMode = (FLASH->OPTR & FLASH_OPTR_DBANK) ? 1U : 0U;

  return (flashBankMode);
}
#endif /* FLASH_MEM */


/*
 * Get Flash Bank Number
 *    Parameter:      adr:  Sector Address
 *    Return Value:   Bank Number (0..1)
 */

#if defined FLASH_MEM
static u32 GetFlashBankNum(u32 adr) {
  u32 flashBankNum;

  if (flashType == 1U) {
    /* Dual-Bank Flash */
    if ((flashBankMode == 1U) || (flashSize == 0x80000)) { /* 512kB devices us always 2 bank numbers */
      /* Dual-Bank Flash configured as Dual-Bank */
      if (adr >= (flashBase + flashBankSize)) {
        flashBankNum = 1U;
      }
      else {
        flashBankNum = 0U;
      }
    }
    else {
      /* Dual-Bank Flash configured as Single-Bank */
      flashBankNum = 0U;
    }
  }
  else {
    /* Single-Bank Flash */
    flashBankNum = 0u;
  }

  return (flashBankNum);
}
#endif /* FLASH_MEM */


/*
 * Get Flash Page Number
 *    Parameter:      adr:  Page Address
 *    Return Value:   Page Number (0..127)
 */

#if defined FLASH_MEM
static u32 GetFlashPageNum (unsigned long adr) {
  u32 flashPageNum;

  if (flashType == 1U) {
    /* Dual-Bank Flash */
    if ((flashBankMode == 1U) || (flashSize == 0x80000)) { /* 512kB devices us always 2 bank numbers */
      /* Dual-Bank Flash configured as Dual-Bank */
      flashPageNum = (((adr & (flashBankSize - 1U)) ) >> 11); /* 2K sector size */
    }
    else {
      /* Dual-Bank Flash configured as Single-Bank */
      flashPageNum = (((adr & (flashSize     - 1U)) ) >> 11); /* 2K sector size */
    }
  }
  else {
      /* Single-Bank Flash */
      flashPageNum = (((adr & (flashSize     - 1U)) ) >> 11); /* 2K sector size */
  }

  return (flashPageNum);
}
#endif /* FLASH_MEM */


/*
 *  Initialize Flash Programming Functions
 *    Parameter:      adr:  Device Base Address
 *                    clk:  Clock Frequency (Hz)
 *                    fnc:  Function Code (1 - Erase, 2 - Program, 3 - Verify)
 *    Return Value:   0 - OK,  1 - Failed
 */

int Init (unsigned long adr, unsigned long clk, unsigned long fnc) {

  __disable_irq();                                       /* Disable all interrupts */

  FLASH->KEYR = FLASH_KEY1;                              /* Unlock Flash operation */
  FLASH->KEYR = FLASH_KEY2;

#ifdef FLASH_OPT
  FLASH->OPTKEYR  = FLASH_OPTKEY1;                       /* Unlock Option Bytes operation */
  FLASH->OPTKEYR  = FLASH_OPTKEY2;
#endif /* FLASH_OPT */

  /* Wait until the flash is ready */
  while (FLASH->SR & FLASH_SR_BSY) __NOP();

//FLASH->ACR  = 0x00000000;                              /* Zero Wait State, no Cache, no Prefetch */

#if defined FLASH_MEM
  flashBase = adr;
  flashSize = ((*((u32 *)FLASHSIZE_BASE)) & 0xFFFFU) << 10;
  flashBankSize = flashSize >> 1;
  flashType = GetFlashType();
	flashBankMode = GetFlashBankMode();
#endif /* FLASH_MEM */

  if ((FLASH->OPTR & FLASH_OPTR_IDWG_SW) == 0U) {        /* Test if IWDG is running (IWDG in HW mode) */
    /* Set IWDG time out to ~32.768 second */
    IWDG->KR  = 0xAAAA; 
    IWDG->KR  = 0x5555;                                  /* Enable write access to IWDG_PR and IWDG_RLR */
    IWDG->PR  = 6;                                       /* Set prescaler to 256 */
    IWDG->RLR = 4095;                                    /* Set reload value to 4095 */
  }

  if ((FLASH->OPTR & FLASH_OPTR_WWDG_SW) == 0U) {        /* Test if WWDG is running (WWDG_SW in HW mode) */
    WWDG->CFR = 0x1FF;
    WWDG->CR  = 0x7F;
  }

  return (0);
}


/*
 *  De-Initialize Flash Programming Functions
 *    Parameter:      fnc:  Function Code (1 - Erase, 2 - Program, 3 - Verify)
 *    Return Value:   0 - OK,  1 - Failed
 */

int UnInit (unsigned long fnc) {

  if ((* (u32 *)flashBase) != 0xFFFFFFFFU) {
    FLASH->ACR &= ~(FLASH_ACR_EMPTY);                    /* Set Flash Empty bit */
  }

  FLASH->CR |= FLASH_CR_LOCK;                            /* Lock Flash operation */
  __DSB();

#ifdef FLASH_OPT
  FLASH->CR  = FLASH_CR_OBL_LAUNCH;                      /* Load option bytes */
  __DSB();
  while (FLASH->CR & FLASH_CR_OBL_LAUNCH);

  FLASH->CR = FLASH_CR_OPTLOCK;                          /* Lock option bytes operation */
  __DSB();
#endif /* FLASH_OPT */

  return (0);
}


/*
 *  Blank Check Checks if Memory is Blank
 *    Parameter:      adr:  Block Start Address
 *                    sz:   Block Size (in bytes)
 *                    pat:  Block Pattern
 *    Return Value:   0 - OK,  1 - Failed
 */

int BlankCheck (unsigned long adr, unsigned long sz, unsigned char pat) {
  /* force erase even if the content is 'Initial Content of Erased Memory'.
     Only a erased sector can be programmed. I think this is because of ECC */
  return (1);
}


/*
 *  Erase complete Flash Memory
 *    Return Value:   0 - OK,  1 - Failed
 */

#if defined FLASH_MEM
int EraseChip (void) {

  FLASH->SR  = FLASH_PGERR;                              /* Reset Error Flags */

  FLASH->CR  = (FLASH_CR_MER1 | FLASH_CR_MER2);          /* Bank A/B mass erase enabled */
  FLASH->CR |=  FLASH_CR_STRT;                           /* Start erase */
  __DSB();

  while (FLASH->SR & FLASH_SR_BSY) __NOP();

  if (FLASH->SR & FLASH_PGERR) {                         /* Check for Error */
    FLASH->SR  = FLASH_PGERR;                            /* Reset Error Flags */
    return (1);                                          /* Failed */
  }

  return (0);                                            /* Done */
}
#endif /* FLASH_MEM */

#ifdef FLASH_OPT
int EraseChip (void) {

  FLASH->SR  = FLASH_PGERR;                              /* Reset Error Flags */

#if defined STM32G0x1
#endif

#if defined STM32G0x0
  FLASH->OPTR      = 0xFFFFFEAA;                        /* Write OPTR reset value */
  FLASH->WRP1AR    = 0x0000007F;                        /* Write WRP1AR reset value */
  FLASH->WRP1BR    = 0x0000007F;                        /* Write WRP1BR reset value */
#if defined FLASH_DB
  FLASH->WRP2AR    = 0x0000007F;                        /* Write WRP1AR reset value */
  FLASH->WRP2BR    = 0x0000007F;                        /* Write WRP1BR reset value */
#endif
#endif // STM32G0x0

#if defined STM32G0x1
  FLASH->OPTR      = 0xFFFFFEAA;                        /* Write OPTR reset value */
  FLASH->WRP1AR    = 0x000000FF;                        /* Write WRP1AR reset value */
  FLASH->WRP1BR    = 0x000000FF;                        /* Write WRP1BR reset value */
  FLASH->PCROP1ASR = 0x000001FF;                        /* Write PCROP1ASR reset value */
  FLASH->PCROP1AER = 0x00000000;                        /* Write PCROP1AER reset value */
  FLASH->PCROP1BSR = 0x000001FF;                        /* Write PCROP1BSR reset value */
  FLASH->PCROP1BER = 0x00000000;                        /* Write PCROP1BER reset value */
  FLASH->SECR      = 0x00000000;                        /* Write SECR reset value */
#if defined FLASH_DB
  FLASH->WRP2AR    = 0x000000FF;                        /* Write WRP1AR reset value */
  FLASH->WRP2BR    = 0x000000FF;                        /* Write WRP1BR reset value */
  FLASH->PCROP2ASR = 0x000001FF;                        /* Write PCROP1ASR reset value */
  FLASH->PCROP2AER = 0x00000000;                        /* Write PCROP1AER reset value */
  FLASH->PCROP2BSR = 0x000001FF;                        /* Write PCROP1BSR reset value */
  FLASH->PCROP2BER = 0x00000000;                        /* Write PCROP1BER reset value */
#endif
#endif // STM32G0x1

  FLASH->CR       = FLASH_CR_OPTSTRT;                    /* Program values */
  __DSB();

  while (FLASH->SR & FLASH_SR_BSY) __NOP();

  if (FLASH->SR & FLASH_PGERR) {                         /* Check for Error */
    FLASH->SR  = FLASH_PGERR;                            /* Reset Error Flags */
    return (1);                                          /* Failed */
  }

  return (0);                                            /* Done */
}
#endif /* FLASH_OPT */


/*
 *  Erase Sector in Flash Memory
 *    Parameter:      adr:  Sector Address
 *    Return Value:   0 - OK,  1 - Failed
 */

#if defined FLASH_MEM
int EraseSector (unsigned long adr) {
  u32 b, p;

  b = GetFlashBankNum(adr);                              /* Get Bank Number 0..1  */
  p = GetFlashPageNum(adr);                              /* Get Page Number 0..x */

  FLASH->SR  = FLASH_PGERR;                              /* Reset Error Flags */

  FLASH->CR  = (FLASH_CR_PER |                           /* Page Erase Enabled */
                (p <<  3)    |                           /* page Number. 0 to x for each bank */
                (b << 13)     );
  FLASH->CR |=  FLASH_CR_STRT;                           /* Start Erase */
  __DSB();

  while (FLASH->SR & FLASH_SR_BSY) __NOP();

  if (FLASH->SR & FLASH_PGERR) {                         /* Check for Error */
    FLASH->SR  = FLASH_PGERR;                            /* Reset Error Flags */
    return (1);                                          /* Failed */
  }

  return (0);                                            /* Done */
}
#endif /* FLASH_MEM */

#if defined FLASH_OPT || defined FLASH_OTP
int EraseSector (unsigned long adr) {
  /* erase sector is not needed for
     - Flash Option bytes
     - Flash One Time Programmable bytes
  */
  return (0);                                            /* Done */
}
#endif /* FLASH_OPT || defined FLASH_OTP */


/*
 *  Program Page in Flash Memory
 *    Parameter:      adr:  Page Start Address
 *                    sz:   Page Size
 *                    buf:  Page Data
 *    Return Value:   0 - OK,  1 - Failed
 */

#if defined FLASH_MEM || defined FLASH_OTP
int ProgramPage (unsigned long adr, unsigned long sz, unsigned char *buf) {

  sz = (sz + 7) & ~7;                                    /* Adjust size for two words */

  FLASH->SR  = FLASH_PGERR;                              /* Reset Error Flags */

  FLASH->CR = FLASH_CR_PG;                               /* Programming Enabled */

  while (sz) {
    M32(adr    ) = *((u32 *)(buf + 0));                  /* Program the first word of the Double Word */
    M32(adr + 4) = *((u32 *)(buf + 4));                  /* Program the second word of the Double Word */
    __DSB();

    while (FLASH->SR & FLASH_SR_BSY) __NOP();

    if (FLASH->SR & FLASH_PGERR) {                       /* Check for Error */
      FLASH->SR  = FLASH_PGERR;                          /* Reset Error Flags */
      return (1);                                        /* Failed */
    }

    adr += 8;                                            /* Go to next DoubleWord */
    buf += 8;
    sz  -= 8;
  }

  FLASH->CR &= ~(FLASH_CR_PG) ;                          /* Reset CR */

  return (0);
}
#endif /* FLASH_MEM || FLASH_OTP */

#ifdef FLASH_OPT
int ProgramPage (unsigned long adr, unsigned long sz, unsigned char *buf) {
#if defined STM32G0x0
  u32 optr      = *((u32 *)(buf +  0));
  u32 wrp1ar    = *((u32 *)(buf +  4));
  u32 wrp1br    = *((u32 *)(buf +  8));
#if defined FLASH_DB
  u32 wrp2ar    = *((u32 *)(buf + 12));
  u32 wrp2br    = *((u32 *)(buf + 16));
#endif
#endif /* STM32G0x0 */

#if defined STM32G0x1
  u32 optr      = *((u32 *)(buf +  0));
  u32 wrp1ar    = *((u32 *)(buf +  4));
  u32 wrp1br    = *((u32 *)(buf +  8));
  u32 pcrop1asr = *((u32 *)(buf + 12));
  u32 pcrop1aer = *((u32 *)(buf + 16));
  u32 pcrop1bsr = *((u32 *)(buf + 20));
  u32 pcrop1ber = *((u32 *)(buf + 24));
  u32 secr      = *((u32 *)(buf + 28));
#if defined FLASH_DB
  u32 wrp2ar    = *((u32 *)(buf + 32));
  u32 wrp2br    = *((u32 *)(buf + 36));
  u32 pcrop2asr = *((u32 *)(buf + 40));
  u32 pcrop2aer = *((u32 *)(buf + 44));
  u32 pcrop2bsr = *((u32 *)(buf + 48));
  u32 pcrop2ber = *((u32 *)(buf + 52));
#endif
#endif /* STM32G0x1 */

  FLASH->SR  = FLASH_PGERR;                             /* Reset Error Flags */

#if defined STM32G0x0
  FLASH->OPTR      = (optr      & 0x3F7FFFFF);          // Write OPTR values
  FLASH->WRP1AR    = (wrp1ar    & 0x007F007F);          // Write WRP1AR values
  FLASH->WRP1BR    = (wrp1br    & 0x007F007F);          // Write WRP1BR values
#if defined FLASH_DB
  FLASH->WRP2AR    = (wrp2ar    & 0x007F007F);          // Write WRP2AR values
  FLASH->WRP2BR    = (wrp2br    & 0x007F007F);          // Write WRP2BR values
#endif
#endif /* STM32G0x0 */

#if defined STM32G0x1
  FLASH->OPTR      = (optr      & 0x3F7FFFFF);          // Write OPTR values
  FLASH->WRP1AR    = (wrp1ar    & 0x007F007F);          // Write WRP1AR values
  FLASH->WRP1BR    = (wrp1br    & 0x007F007F);          // Write WRP1BR values
  FLASH->PCROP1ASR = (pcrop1asr & 0x000001FF);          // Write PCROP1ASR values
  FLASH->PCROP1AER = (pcrop1aer & 0x800001FF);          // Write PCROP1AER values
  FLASH->PCROP1BSR = (pcrop1bsr & 0x000001FF);          // Write PCROP1BSR values
  FLASH->PCROP1BER = (pcrop1ber & 0x000001FF);          // Write PCROP1BER values
  FLASH->SECR      = (secr      & 0x0FF100FF);          // Write SECR values
#if defined FLASH_DB
  FLASH->WRP2AR    = (wrp2ar    & 0x007F007F);          // Write WRP2AR values
  FLASH->WRP2BR    = (wrp2br    & 0x007F007F);          // Write WRP2BR values
  FLASH->PCROP2ASR = (pcrop2asr & 0x000001FF);          // Write PCROP2ASR values
  FLASH->PCROP2AER = (pcrop2aer & 0x800001FF);          // Write PCROP2AER values
  FLASH->PCROP2BSR = (pcrop2bsr & 0x000001FF);          // Write PCROP2BSR values
  FLASH->PCROP2BER = (pcrop2ber & 0x000001FF);          // Write PCROP2BER values
#endif
#endif /* STM32G0x0 */

  FLASH->CR  = FLASH_CR_OPTSTRT;                        /* Program values */
  __DSB();

  while (FLASH->SR & FLASH_SR_BSY) __NOP();

  if (FLASH->SR & FLASH_PGERR) {                        /* Check for Error */
    FLASH->SR |= FLASH_PGERR;                           /* Reset Error Flags */
    return (1);                                         /* Failed */
  }

  return (0);                                           /* Done */
}
#endif /* FLASH_OPT */


/*
 *  Verify Flash Contents
 *    Parameter:      adr:  Start Address
 *                    sz:   Size (in bytes)
 *                    buf:  Data
 *    Return Value:   (adr+sz) - OK, Failed Address
 */

#ifdef FLASH_OPT
unsigned long Verify (unsigned long adr, unsigned long sz, unsigned char *buf) {
#if 0 /* verify does not work because I do not get the registers updated without a reset! */
#if defined STM32G0x0
  u32 optr      = *((u32 *)(buf +  0));
  u32 wrp1ar    = *((u32 *)(buf +  4));
  u32 wrp1br    = *((u32 *)(buf +  8));
#if defined FLASH_DB
  u32 wrp2ar    = *((u32 *)(buf + 12));
  u32 wrp2br    = *((u32 *)(buf + 16));
#endif
#endif // STM32G0x0

#if defined STM32G0x1
  u32 optr      = *((u32 *)(buf +  0));
  u32 wrp1ar    = *((u32 *)(buf +  4));
  u32 wrp1br    = *((u32 *)(buf +  8));
  u32 pcrop1asr = *((u32 *)(buf + 12));
  u32 pcrop1aer = *((u32 *)(buf + 16));
  u32 pcrop1bsr = *((u32 *)(buf + 20));
  u32 pcrop1ber = *((u32 *)(buf + 24));
  u32 secr      = *((u32 *)(buf + 28));
#if defined FLASH_DB
  u32 wrp2ar    = *((u32 *)(buf + 32));
  u32 wrp2br    = *((u32 *)(buf + 36));
  u32 pcrop2asr = *((u32 *)(buf + 40));
  u32 pcrop2aer = *((u32 *)(buf + 44));
  u32 pcrop2bsr = *((u32 *)(buf + 48));
  u32 pcrop2ber = *((u32 *)(buf + 52));
#endif
#endif /* STM32G0x1 */

  FLASH->SR  = FLASH_PGERR;                             // Reset Error Flags
  FLASH->CR  = FLASH_OBL_LAUNCH;                        // Load Option Bytes -> generates a reset!
  __DSB();

#if defined STM32G0x0
  if ((FLASH->OPTR      & 0x3F7FFFFF) != (optr      & 0x3F7FFFFF)) {    // Check OPTR values
    return (adr + 0);
  }

  if ((FLASH->WRP1AR    & 0x007F007F) != (wrp1ar    & 0x007F007F)) {    // Check WRP1AR values
    return (adr + 1);
  }

  if ((FLASH->WRP1BR    & 0x007F007F) != (wrp1br    & 0x007F007F)) {    // Check WRP1BR values
    return (adr + 2);
  }
#if defined FLASH_DB
  if ((FLASH->WRP2AR    & 0x007F007F) != (wrp2ar    & 0x007F007F)) {    // Check WRP2AR values
    return (adr + 3);
  }

  if ((FLASH->WRP2BR    & 0x007F007F) != (wrp2br    & 0x007F007F)) {    // Check WRP2BR values
    return (adr + 4);
  }
#endif
#endif /* STM32G0x0 */

#if defined STM32G0x1
  if ((FLASH->OPTR      & 0x3F7FFFFF) != (optr      & 0x3F7FFFFF)) {    // Check OPTR values
    return (adr + 0);
  }

  if ((FLASH->WRP1AR    & 0x007F007F) != (wrp1ar    & 0x007F007F)) {    // Check WRP1AR values
    return (adr + 1);
  }

  if ((FLASH->WRP1BR    & 0x007F007F) != (wrp1br    & 0x007F007F)) {    // Check WRP1BR values
    return (adr + 2);
  }

  if ((FLASH->PCROP1ASR & 0x000001FF) != (pcrop1asr & 0x000001FF)) {    // Check PCROP1ASR values
    return (adr + 3);
  }

  if ((FLASH->PCROP1AER & 0x800001FF) != (pcrop1aer & 0x800001FF)) {    // Check PCROP1AER values
    return (adr + 4);
  }

  if ((FLASH->PCROP1BSR & 0x000001FF) != (pcrop1bsr & 0x000001FF)) {    // Check PCROP1BSR values
    return (adr + 5);
  }

  if ((FLASH->PCROP1BER & 0x000001FF) != (pcrop1ber & 0x000001FF)) {    // Check PCROP1BER values
    return (adr + 6);
  }

  if ((FLASH->SECR      & 0x0FF100FF) != (secr      & 0x0FF100FF)) {    // Check SECR values
    return (adr + 7);
  }
#if defined FLASH_DB
  if ((FLASH->WRP2AR    & 0x007F007F) != (wrp2ar    & 0x007F007F)) {    // Check WRP2AR values
    return (adr + 8);
  }

  if ((FLASH->WRP2BR    & 0x007F007F) != (wrp2br    & 0x007F007F)) {    // Check WRP2BR values
    return (adr + 9);
  }

  if ((FLASH->PCROP2ASR & 0x000001FF) != (pcrop2asr & 0x000001FF)) {    // Check PCROP2ASR values
    return (adr + 10);
  }

  if ((FLASH->PCROP2AER & 0x800001FF) != (pcrop2aer & 0x800001FF)) {    // Check PCROP2AER values
    return (adr + 11);
  }

  if ((FLASH->PCROP2BSR & 0x000001FF) != (pcrop2bsr & 0x000001FF)) {    // Check PCROP2BSR values
    return (adr + 12);
  }

  if ((FLASH->PCROP2BER & 0x000001FF) != (pcrop2ber & 0x000001FF)) {    // Check PCROP2BER values
    return (adr + 13);
  }
#endif
#endif /* STM32G0x1 */

#endif
  return (adr + sz);
}
#endif /* FLASH_OPT */
