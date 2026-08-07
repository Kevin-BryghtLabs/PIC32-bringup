#include "bl_piece_id.hpp"
#include "bl_piece_id_freq_calc.hpp"
#include "peripheral/tcc/plib_tcc0.h"
#include "pic32cm6408jh00048.h"

void initPll() {
    // Enable GClk 8
    GCLK_REGS->GCLK_PCHCTRL[0] = GCLK_PCHCTRL_GEN(0x8UL)  | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[0] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    // Configure filter to High-Damping to reduce jitter from fractional PLL
    //   Also default lock time and Gclk reference
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(0UL) | OSCCTRL_DPLLCTRLB_LTIME(0UL)| OSCCTRL_DPLLCTRLB_REFCLK(2UL) ;

    /* Selection of the DPLL Pre-Scalar */
   OSCCTRL_REGS->OSCCTRL_DPLLPRESC = (uint8_t)OSCCTRL_DPLLPRESC_PRESC(0UL);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk )
    {
        /* Waiting for the synchronization */
    }
}

void disablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(OSCCTRL_DPLLCTRLA_ONDEMAND_Msk );

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

void enablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(OSCCTRL_DPLLCTRLA_ENABLE_Msk | OSCCTRL_DPLLCTRLA_ONDEMAND_Msk );

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

void setPllMult(uint16_t whole, uint8_t frac) {
    OSCCTRL_REGS->OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(frac) | OSCCTRL_DPLLRATIO_LDR(whole);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
    {
        /* Waiting for the synchronization */
    }
}

void configPLL(const PllTccConfig & config) {
    disablePll();
    const uint32_t whole = config.ldr / 16 - 1;
    const uint32_t frac = config.ldr % 16;
    setPllMult(whole, frac);
    enablePll();
}

void configTCC(const PllTccConfig & config) {

    TCC0_PWMStop();

    GCLK_REGS->GCLK_GENCTRL[8] = GCLK_GENCTRL_DIV(config.gclkDiv) | GCLK_GENCTRL_SRC(6UL) | GCLK_GENCTRL_GENEN_Msk;

    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk)
    {
        /* wait for the Generator 8 synchronization */
    }

    uint32_t period = config.tccCnt;
    if (period & 1) {
        period++;
    }

    TCC0_REGS->TCC_CC[1] = period / 2;
    TCC0_REGS->TCC_PER = period;

    while (TCC0_REGS->TCC_SYNCBUSY != 0U)
    {
        /* Wait for sync */
    }

    TCC0_PWMStart();
}

void initPieceId() {
    initPll();
}

void startPieceId() {
    uint32_t scanFreq = 1'500'000;

    //const PllTccConfig config = getPllTccConfigForFreq(scanFreq);
    PllTccConfig config = {
        .gclkDiv = 24,
        .ldr     = ((44 + 1) * 16) + 12,
        .tccCnt  = 61,
    };

    configPLL(config);
    configTCC(config);
}
