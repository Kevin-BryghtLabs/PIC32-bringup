#include "bl_piece_id.hpp"

#include "bl_ble_packet_headers.h"
#include "bl_piece_id_freq_calc.hpp"
#include "bl_shu_stats.hpp"
#include "bl_uart.h"
#include "hub_log.h"
#include "peripheral/adc/plib_adc0.h"
#include "peripheral/port/plib_port.h"
#include "peripheral/tc/plib_tc3.h"
#include "peripheral/tcc/plib_tcc0.h"
#include "peripheral/tcc/plib_tcc1.h"
#include "pic32cm6408jh00048.h"
#include "timer.h"
#include <cstdlib>
#include <cinttypes>
#include <limits>

// The Clock chain is:
//
// 48MHz OSC -> (Gclkdiv) GCLK8 (dynamic control to get target freq)
// GCLK8     -> PLL  (48Mhz -> 96Mhz)  Div 1
// PLL       -> GClk2 (Peripherals are driven by GClks, not PLL directly)
// GClk2     -> TCC0 & TCC1
// TCC0 count and period determine the waveform
// TCC1 driven by same clock and generates the Hard Fault event to stop TCC0 waveform
//
// GClk8 divider, PLL Mult, and TCC count are variables used to generate the
// target frequency.  (See bl_piece_id_freq_calc.cpp)
//
///////////////////////////////////////////////////////////////////////////////////////
//
// TCC0 PWM: .....|^|_|^|_|^|_|^|_|^|_|^|_|^|_|^|_|^|_|^|_
//
// TCC1:     .....|---------------------------------------|
//
// TC3:      .............................................|-----@----@......
//
// DrainEn:  ___|^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^|___________
//
// ADC:                                                              S*****D
//
// TCC0 outputs its PWM signal as the TX pulse train for PieceId.
// TCC1 is driven by the same clock and starts at the same time.  It counts the time until
//   10 pulses have been generated on TCC0, then triggers a HW Event (A).
// HW Event (A) on TCC0 generates a hard fault, stopping the timer and forcing the output
//   low.  Hard fault is used because it is faster than events that have to synchronize to
//   the TCC clock domain and it forces the output to a known state.
// HW Event (A) triggers TC3 to start.  TC3 is configured with two Match/Compare counts,
//   generating HW Events (B) and (C)
// HW Event (B) causes the GPIO (PORT) controller to set the DrainEn GPIO low
// HW Event (C) triggers a conversion on the ADC
// The ADC generates an interrupt when conversion is complete, setting a flag.
// Non-ISR software checks the flag, reads the value from the ADC and sets up the next
//   frequency scan if necessary
// 
///////////////////////////////////////////////////////////////////////////////////////
///
// GClk notes
//   - "Before a Generator is enabled, the corresponding clock source should be enabled."
//   - "The Generator must be enabled (GENCTRLn.GENEN = 1) and the division factor must be
//      set (GENTRLn.DIVSEL and GENCTRLn.DIV) by performing a single 32-bit write to the
//      Generator Control register (GENCTRLn)."
//   - "Before switching the Generic Clock Generator 0 (GCLKGEN0) from a clock source A to
//      another clock source B, enable the ONDEMAND feature of the clock source A to ensure
//      a proper transition from clock source A to clock source B."
//      (This shouldn't affect us, as the SOURCE is not changing)
//   - "When dividing a clock with an odd division factor, the duty-cycle will not be 50/50.
//      Setting the Improve Duty Cycle bit of the Generator Control register (GENCTRLn.IDC)
//      will result in a 50/50 duty cycle."
//
// PLL notes
//   - "The frequency of the DPLL output clock CK is stable when the module is enabled and
//      when the Lock bit in the DPLL Status register is set (DPLLSTATUS.LOCK)"
//   - "The lock time timer uses GCLK_DPLL_32 as a source clock, which must be enabled and
//      configured as a 32.768 kHz clock"
//   - "When a software operation requires reference clock switching, the recommended
//      procedure is to turn the DPLL into the Standby mode, modify the DPLLCTRLB.REFCLK to
//      select the desired reference source, and activate the DPLL again."
//      (I believe this is the SOURCE, not just changing reference frequency)
//   - "The DPLL Controller supports on-the-fly update of the DPLL Ratio Control (DPLLRATIO)
//      register only when the GCLK_DPLL_32K (GCLK.PCHCTRL1) is configured and enabled.  The
//      FDPLL reference clock (DPLLCTRLB.REFCLK) can still be set to one of the three
//      options (XOSC, XOSC32K, GCLK) as long as GCLK_DPLL_32K is active.
//   - "The following bits need synchronization when written:
//        • Enable bit in control register A (DPLLCTRLA.ENABLE)
//        • DPLL Ratio register (DPLLRATIO)
//        • DPLL Prescaler register (DPLLPRESC)"
//   - "The Pattern (PATT), Period (PER) and Compare Channels (CCx) registers are all double
//      buffered.  ... the double buffering feature is not mandatory. The double buffering
//      is disabled by writing a '1' to CTRLSET.LUPD."
//

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

// Top-level "master" mux
static constexpr PORT_PIN MMA0_PIN    = PORT_PIN_PA08;
static constexpr PORT_PIN MMA1_PIN    = PORT_PIN_PA09;
static constexpr PORT_PIN MMA2_PIN    = PORT_PIN_PA10;
static constexpr uint32_t MMA0_BIT    = (1 << MMA0_PIN);
static constexpr uint32_t MMA1_BIT    = (1 << MMA1_PIN);
static constexpr uint32_t MMA2_BIT    = (1 << MMA2_PIN);

// Second-level "slave" mux
static constexpr PORT_PIN MSA0_PIN    = PORT_PIN_PA11;
static constexpr PORT_PIN MSA1_PIN    = PORT_PIN_PA16;
static constexpr PORT_PIN MSA2_PIN    = PORT_PIN_PA17;
static constexpr uint32_t MSA0_BIT    = (1 << MSA0_PIN);
static constexpr uint32_t MSA1_BIT    = (1 << MSA1_PIN);
static constexpr uint32_t MSA2_BIT    = (1 << MSA2_PIN);

static constexpr uint32_t MMA_MASK = MMA0_BIT | MMA1_BIT | MMA2_BIT;
static constexpr uint32_t MSA_MASK = MSA0_BIT | MSA1_BIT | MSA2_BIT;

// The board PieceId nets look like this in a top-down view:
//
//   S33 S40 S41 S48 S52 S53 S60 S61
//   S34 S39 S42 S47 S51 S54 S59 S62
//   S35 S38 S43 S46 S50 S55 S58 S63
//   S36 S37 S44 S45 S49 S56 S57 S64
//   S1  S8  S9  S16 S20 S21 S28 S29
//   S2  S7  S10 S15 S19 S22 S27 S30
//   S3  S6  S11 S14 S18 S23 S26 S31
//   S4  S5  S12 S13 S17 S24 S25 S32

// Reducing each by 1 to be 0-indexed instead of 1-indexed
// and flipping top to bottom so that spot 0 == A1, we get:
static uint8_t spotToNetMap[] = {
  3,  4, 11, 12, 16, 23, 24, 31,
  2,  5, 10, 13, 17, 22, 25, 30,
  1,  6,  9, 14, 18, 21, 26, 29,
  0,  7,  8, 15, 19, 20, 27, 28,
 35, 36, 43, 44, 48, 55, 56, 63,
 34, 37, 42, 45, 49, 54, 57, 62,
 33, 38, 41, 46, 50, 53, 58, 61,
 32, 39, 40, 47, 51, 52, 59, 60,
};
static_assert(sizeof(spotToNetMap) == 64);

static const uint32_t MMABitsForValue[] = {
         0 |        0 |        0,
         0 |        0 | MMA0_BIT,
         0 | MMA1_BIT |        0,
         0 | MMA1_BIT | MMA0_BIT,
  MMA2_BIT |        0 |        0,
  MMA2_BIT |        0 | MMA0_BIT,
  MMA2_BIT | MMA1_BIT |        0,
  MMA2_BIT | MMA1_BIT | MMA0_BIT,
};

static const uint32_t MSABitsForValue[] = {
         0 |        0 |        0,
         0 |        0 | MSA0_BIT,
         0 | MSA1_BIT |        0,
         0 | MSA1_BIT | MSA0_BIT,
  MSA2_BIT |        0 |        0,
  MSA2_BIT |        0 | MSA0_BIT,
  MSA2_BIT | MSA1_BIT |        0,
  MSA2_BIT | MSA1_BIT | MSA0_BIT,
};

struct FrequencyData {
  uint32_t frequency;
  uint16_t drainDelayCounts;
  uint16_t ringTimeCounts;
  PllTccConfig config;
};

#if 0
// List of frequencies used for testing parameter calculation
FrequencyData frequencies[30] = {
  { 1'000'000, 64, 255, {} },
  { 1'500'000, 43, 170, {} },
  { 2'000'000, 32, 128, {} },
  { 2'500'000, 26, 102, {} },
  { 3'000'000, 22,  85, {} },
  { 3'500'000, 19,  73, {} },
  { 4'000'000, 16,  64, {} },
  { 4'500'000, 15,  57, {} },
  { 5'000'000, 13,  51, {} },
  { 5'500'000, 12,  47, {} },
  { 6'000'000, 11,  43, {} },
  { 6'500'000, 10,  40, {} },
  { 7'000'000, 10,  37, {} },
  { 7'500'000,  9,  34, {} },
  { 8'000'000,  8,  32, {} },
  { 8'500'000,  8,  30, {} },
  { 9'000'000,  8,  29, {} },
  { 9'500'000,  7,  27, {} },
};
#endif
#if 0
// Claude-generated frequency list, characterized by DO_SCAN
FrequencyData frequencies[] = {
  /* W Knight - 183  */ { 1'960'000, 30,   2, {} },
  /* B Knight - 150  */ { 2'150'000, 30,   3, {} },
  /* W Rook   - 133  */ { 2'300'000, 30,   3, {} },
  /* B Queen  - 110  */ { 2'480'000, 25,   3, {} },
  /* W Pawn   - 94   */ { 2'750'000, 26,   6, {} },
  /* B King   - 80   */ { 2'850'000, 27,   4, {} },
  /* W Bishop - 68   */ { 3'150'000, 25,   5, {} },
  /* B Rook   - 57   */ { 3'380'000, 35,   4, {} },
  /* W Queen  - 49   */ { 3'620'000, 28,   6, {} },
  /* B Pawn   - 43   */ { 3'900'000, 34,   5, {} },
  /* W King   - 35.2 */ { 4'200'000, 30,   3, {} },
  /* B Bishop - 30   */ { 4'520'000, 34,   6, {} },
};
#endif
#if 0
// Claude-generated frequency list, calculated driving frequency
FrequencyData frequencies[] = {
  /* W Knight - 183  */ { 1'960'000, 30,   2, {} },
  /* B Knight - 150  */ { 2'151'000, 30,   3, {} },
  /* W Rook   - 133  */ { 2'312'000, 30,   3, {} },
  /* B Queen  - 110  */ { 2'489'000, 25,   3, {} },
  /* W Pawn   - 94   */ { 2'678'000, 26,   6, {} },
  /* B King   - 80   */ { 2'882'000, 27,   4, {} },
  /* W Bishop - 68   */ { 3'100'000, 25,   5, {} },
  /* B Rook   - 57   */ { 3'332'000, 35,   4, {} },
  /* W Queen  - 49   */ { 3'585'000, 28,   6, {} },
  /* B Pawn   - 43   */ { 3'854'000, 34,   5, {} },
  /* W King   - 35.2 */ { 4'149'000, 30,   3, {} },
  /* B Bishop - 30   */ { 4'464'000, 34,   6, {} },
};
#endif
#if 0
FrequencyData frequencies[] = {
  /* W Knight -  0 - 271   */ { 1'624'000, 30,   2, {} },
  /* B Knight -  1 - 223   */ { 1'785'000, 30,   3, {} },
  /* W Rook   -  2 - 187.5 */ { 1'941'000, 30,   3, {} },
  /* B Queen  -  3 - 153   */ { 2'140'000, 25,   3, {} },
  /* W Pawn   -  4 - 125   */ { 2'355'000, 26,   6, {} },
  /* B King   -  5 - 104   */ { 2'567'000, 27,   4, {} },
  /* W Bishop -  6 - 84    */ { 2'834'000, 25,   5, {} },
  /* B Rook   -  7 - 69.5  */ { 3'089'000, 35,   4, {} },
  /* W Queen  -  8 - 56    */ { 3'402'000, 28,   6, {} },
  /* B Pawn   -  9 - 46    */ { 3'707'000, 34,   5, {} },
  /* W King   - 10 - 37    */ { 4'064'000, 30,   3, {} },
  /* B Bishop - 11 - 29.5  */ { 4'458'000, 34,   6, {} },
};
#endif
#if 0
// Tested and tuned with NT
FrequencyData frequencies[] = {
  /* W Knight -  0 - 271   */ { 1'624'000, 25, 5, {} },
  /* B Knight -  1 - 223   */ { 1'785'000, 24, 5, {} },
  /* W Rook   -  2 - 187.5 */ { 1'941'000, 24, 5, {} },
  /* B Queen  -  3 - 153   */ { 2'140'000, 24, 5, {} },
  /* W Pawn   -  4 - 125   */ { 2'355'000, 23, 5, {} },
  /* B King   -  5 - 104   */ { 2'567'000, 20, 5, {} },
  /* W Bishop -  6 - 84    */ { 2'834'000, 18, 5, {} },
  /* B Rook   -  7 - 69.5  */ { 3'089'000, 18, 5, {} },
  /* W Queen  -  8 - 56    */ { 3'402'000, 18, 5, {} },
  /* B Pawn   -  9 - 46    */ { 3'707'000, 17, 5, {} },
  /* W King   - 10 - 37    */ { 4'064'000, 15, 5, {} },
  /* B Bishop - 11 - 29.5  */ { 4'458'000, 15, 5, {} },
};
#endif
// 2 more frequencies on low end
FrequencyData frequencies[] = {
  /* W Knight -  0 - 398   */ { 1'340'000, 26, 5, {} },
  /* B Knight -  1 - 330   */ { 1'476'000, 26, 5, {} },
  /* W Rook   -  2 - 271   */ { 1'624'000, 25, 5, {} },
  /* B Queen  -  3 - 223   */ { 1'785'000, 24, 5, {} },
  /* W Pawn   -  4 - 187.5 */ { 1'941'000, 24, 5, {} },
  /* B King   -  5 - 153   */ { 2'140'000, 24, 5, {} },
  /* W Bishop -  6 - 125   */ { 2'355'000, 23, 5, {} },
  /* B Rook   -  7 - 104   */ { 2'567'000, 20, 5, {} },
  /* W Queen  -  8 - 84    */ { 2'834'000, 18, 5, {} },
  /* B Pawn   -  9 - 69.5  */ { 3'089'000, 18, 5, {} },
  /* W King   - 10 - 56    */ { 3'402'000, 18, 5, {} },
  /* B Bishop - 11 - 46    */ { 3'707'000, 17, 5, {} },
};

static size_t FrequencyCount = ARRAY_SIZE(frequencies);

// Scans these spots first if any have been requested (lsb first)
static uint64_t priorityScanBitBoard;

// Scans these when nothing is set in the priority bitboard (lsb first)
static uint64_t defaultScanBitBoard;

// ADC values for each frequency on a single square
static uint8_t pieceIdResult[ARRAY_SIZE(frequencies)];

// Which frequency we are currently scanning
static uint8_t frequencyIdx;

// Which spot (square) we are currently scanning
static uint8_t activeSpot;

// Is PieceId scanning right now
static bool pieceIdRunning;

// Flag set by ADC ISR
static volatile bool adcDone;

// Stat calculation
static uint32_t pieceIdStartTicks = 0;
static uint32_t pieceIdReadyTicks = 0;

static bool isSamePllConfig(const PllTccConfig & a, const PllTccConfig & b) {
  return a.gclkDiv == b.gclkDiv
    && a.ldrWhole == b.ldrWhole
    && a.ldrFrac == b.ldrFrac;
}

static void configPinsForPieceId(void)
{
    static constexpr PORT_PIN muxPins[] = {
      MMA0_PIN, MMA1_PIN, MMA2_PIN,
      MSA0_PIN, MSA1_PIN, MSA2_PIN,
    };
    static constexpr size_t MuxPinCount = ARRAY_SIZE(muxPins);

    for (unsigned i = 0; i < MuxPinCount; ++i) {
        const PORT_PIN pin = muxPins[i];

        PORT_PinGPIOConfig(pin);
        PORT_PinClear(pin);
        PORT_PinOutputEnable(pin);
    }
}

static void selectMuxPathForSpot(uint8_t spot) {
  if (spot > ARRAY_SIZE(spotToNetMap)) {
    return;
  }

  const uint8_t net = spotToNetMap[spot];
  const uint8_t mma = net / 8;
  const uint8_t msa = net % 8;

  const uint32_t mmaBits = MMABitsForValue[mma];
  const uint32_t msaBits = MSABitsForValue[msa];

  const uint32_t setBits = (mmaBits | msaBits) & (MMA_MASK | MSA_MASK);
  const uint32_t clrBits = (MMA_MASK | MSA_MASK) & ~mmaBits & ~msaBits;

  PORT_REGS->GROUP[0].PORT_OUTSET = setBits;
  PORT_REGS->GROUP[0].PORT_OUTCLR = clrBits;
}

static void enableMuxes() {
  MUX_EN_Set();
}

static void disableMuxes() {
  MUX_EN_Clear();
}

static void adcIrq(ADC_STATUS status, uintptr_t context){
    adcDone = true;
}

static void disableGclk(uint8_t gclkNum) {
    uint32_t val = GCLK_REGS->GCLK_GENCTRL[gclkNum];
    val &= ~(GCLK_GENCTRL_GENEN_Msk);
    GCLK_REGS->GCLK_GENCTRL[gclkNum] = val;

    // Synchronize disable
    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk) {
    }
}

static void setGclkDiv(uint8_t gclkNum, uint8_t divider) {
    uint32_t val = GCLK_REGS->GCLK_GENCTRL[gclkNum];

    // Set divider
    val &= ~GCLK_GENCTRL_DIV_Msk;
    val |= GCLK_GENCTRL_DIV(divider) | GCLK_GENCTRL_GENEN_Msk;

    // Set enable bit and divider in the same write
    GCLK_REGS->GCLK_GENCTRL[gclkNum] = val;

    // Synchronize enable
    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk) {
    }
}

static void initPll() {
    // Enable GClk 8
    GCLK_REGS->GCLK_PCHCTRL[0] = GCLK_PCHCTRL_GEN(0x8UL)  | GCLK_PCHCTRL_CHEN_Msk;
    while ((GCLK_REGS->GCLK_PCHCTRL[0] & GCLK_PCHCTRL_CHEN_Msk) != GCLK_PCHCTRL_CHEN_Msk)
    {
        /* Wait for synchronization */
    }

    // Configure filter to High-Damping to reduce jitter from fractional PLL
    //   Also default lock time and Gclk reference
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLB = OSCCTRL_DPLLCTRLB_FILTER(OSCCTRL_DPLLCTRLB_FILTER_HDFILT_Val) | OSCCTRL_DPLLCTRLB_LTIME(0UL)| OSCCTRL_DPLLCTRLB_REFCLK(2UL) ;

    /* Selection of the DPLL Pre-Scalar */
   OSCCTRL_REGS->OSCCTRL_DPLLPRESC = (uint8_t)OSCCTRL_DPLLPRESC_PRESC(0UL);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLPRESC_Msk )
    {
        /* Waiting for the synchronization */
    }
}

static void setPulseCount(uint8_t pulses, const PllTccConfig & config) {
    if (pulses > 0) {
        pulses -= 1;
    }

    const uint32_t highTimeTicks = config.tccCnt / 2;
    const uint32_t perTicks = config.tccCnt - 1;
    const uint32_t lowTimeTicks = perTicks - highTimeTicks;
    uint32_t eventTimeTicks = 0;
    if (lowTimeTicks < 5) {
        eventTimeTicks = highTimeTicks + 1;
    }
    else {
        eventTimeTicks = highTimeTicks + (lowTimeTicks / 5);
    }
    const uint32_t pulsesTimeInTCCTicks = pulses * config.tccCnt + eventTimeTicks;

    TCC1_REGS->TCC_COUNT = pulsesTimeInTCCTicks;
    //hubLog('I', "TCC1: %u * %lu + %lu = %lu", pulses, config.tccCnt, eventTimeTicks, pulsesTimeInTCCTicks);
    while (TCC1_REGS->TCC_SYNCBUSY != 0U)
    {
        /* Wait for Write Synchronization */
    }
}

static void setUpDrainAndRingTimeTriggers(unsigned freqIdx) {
    // MCC doesn't expose oneshot; set manually
    TC3_REGS->COUNT16.TC_CTRLBSET = TC_CTRLBSET_ONESHOT(1);

    TC3_REGS->COUNT16.TC_CC[0] = frequencies[freqIdx].drainDelayCounts;
    TC3_REGS->COUNT16.TC_CC[1] = frequencies[freqIdx].drainDelayCounts + frequencies[freqIdx].ringTimeCounts;

    while((TC3_REGS->COUNT16.TC_SYNCBUSY) != 0U)
    {
        /* Wait for Write Synchronization */
    }

    // Clear PORTB 23 on event
    PORT_REGS->GROUP[1].PORT_EVCTRL = PORT_EVCTRL_PORTEI0_Msk | PORT_EVCTRL_EVACT0(2) | PORT_EVCTRL_PID0(23);
}

static void setUpSoftwareEventTriggerForTccs() {
    // Manually set "None" generator event so we can start both TCC0 and TCC1 at the same
    // time with a software event.  MCC doesn't support "None".

    // EVSYS channel 1: software-triggered START for TCC0 + TCC1 (no generator)
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0UL)      // NONE
                                | EVSYS_CHANNEL_PATH(2UL)       // asynchronous (required for TCC users)
                                | EVSYS_CHANNEL_EDGSEL(0UL);    // must be 0 on the async path

    // USER.CHANNEL holds channel number + 1
    EVSYS_REGS->EVSYS_USER[9]  = EVSYS_USER_CHANNEL(2UL);        // TCC0 EV0 <- channel 1
    EVSYS_REGS->EVSYS_USER[15] = EVSYS_USER_CHANNEL(2UL);        // TCC1 EV0 <- channel 1
}

static void sendSoftwareEventTriggerForTccs() {
    EVSYS_REGS->EVSYS_SWEVT = EVSYS_SWEVT_CHANNEL1_Msk;
}

static void disablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(0);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

static void enablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(OSCCTRL_DPLLCTRLA_ENABLE_Msk);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

static void setPllMult(uint16_t whole, uint8_t frac) {
    OSCCTRL_REGS->OSCCTRL_DPLLRATIO = OSCCTRL_DPLLRATIO_LDRFRAC(frac) | OSCCTRL_DPLLRATIO_LDR(whole);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk) == OSCCTRL_DPLLSYNCBUSY_DPLLRATIO_Msk)
    {
        /* Waiting for the synchronization */
    }
}

bool isQuiet = false;

static void calculateFrequencyConfigForIndex(unsigned idx) {
  FrequencyData & data = frequencies[idx];

  data.config = getPllTccConfigForFreq(data.frequency);

  if (!isQuiet) {
    hubLog('I', "%2u: F%7" PRIu32 " G%2" PRIu8 " W%2" PRIu16 " f%2" PRIu8 " Tc%3" PRIu32 " Pl%8" PRIu32 " E%+6" PRIi32 " D%2" PRIu16 " R%2" PRIu16,
        idx,
        data.frequency,
        data.config.gclkDiv,
        data.config.ldrWhole,
        data.config.ldrFrac,
        data.config.tccCnt,
        data.config.pllOut,
        data.config.error,
        data.drainDelayCounts,
        data.ringTimeCounts);
  }
}

static void calculateFrequencyConfigs() {
  for (unsigned i = 0; i < FrequencyCount; ++i) {
    WDT_Clear();
    calculateFrequencyConfigForIndex(i);
  }
  WDT_Clear();
}

// Call once to initialize systems after boot
void pieceIdInit() {
    calculateFrequencyConfigs();
    initPll();
}

// Call after captouch or any other task which shares resources with PieceId
static void setupPieceId() {
  ADC0_CallbackRegister(adcIrq, 0);
  ADC0_ChannelSelect(ADC_POSINPUT_AIN1, ADC_NEGINPUT_GND);
  ADC0_Enable();
  configPinsForPieceId();
}

static void stopPieceId() {
  disableMuxes();
  TCC0_PWMStop();
  TCC1_PWMStop();
  TC3_CompareStop();
  ADC0_Disable();
  ADC0_CallbackRegister(nullptr, 0);
}

static uint8_t chooseScanSquare() {
  uint64_t * bitboard = nullptr;

  if (priorityScanBitBoard) {
    bitboard = &priorityScanBitBoard;
  }
  else {
    if (!defaultScanBitBoard) {
      // If no priority squares specified and we have exhausted our
      // default scan, then reset the default scan to all squares
      defaultScanBitBoard = std::numeric_limits<uint64_t>::max();
    }
    bitboard = &defaultScanBitBoard;
  }

  const uint32_t low = *bitboard;
  const uint32_t high = (*bitboard) >> 32;

  uint8_t spot = 0;
  if (low) {
    spot = __builtin_ctz(low);
  }
  else if (high) {
    spot = 32 + __builtin_ctz(high);
  }

  if (spot >= 64) {
    spot = 0;
  }

  // Clear the square we selected
  *bitboard &= ~(1ull << spot);

  return spot;
}

// I tried removing the peripheral resets and making a "2nd and later" setup,
// that just reconfigured without resetting, but it made no difference.  The
// setup time is dominated by the PLL sync, and resets are very fast in
// comparison.
//   - All with resets: 669us
//   - No resets on 2-N: 666us
static void setPieceId(unsigned freqIdx) {
    if (freqIdx >= FrequencyCount) {
        return;
    }

    adcDone = false;
    auto & config = frequencies[freqIdx].config;

    TCC0_PWMStop();
    TCC1_PWMStop();

    bool skipPllInit = false;
    if (freqIdx > 0) {
      skipPllInit = isSamePllConfig(config, frequencies[freqIdx - 1].config);
    }

    if (!skipPllInit) {
      disablePll();
      disableGclk(8);

      setGclkDiv(8, config.gclkDiv);

      const uint32_t whole = config.ldrWhole - 1;
      const uint32_t frac = config.ldrFrac;

      setPllMult(whole, frac);
      enablePll();
    }
    else {
      //hubLog('I', "Same: %u & %u", freqIdx, freqIdx - 1);
    }

    // Do as much of the unrelated setup as we can between enabling PLL and
    // setting up TCC0/TCC1, as the PLL sync takes a long time.  The TCC write
    // syncs will end up waiting for it, so doing other work here makes the
    // full setup time shorter.
    TC3_CompareStop();
    TC3_CompareInitialize();

    setUpDrainAndRingTimeTriggers(freqIdx);

    // Enable does not start counting when an input START event is configured;
    // this timer will not run until triggered by TCC1
    TC3_CompareStart();

    setUpSoftwareEventTriggerForTccs();

    TCC0_PWMInitialize();

    TCC0_REGS->TCC_CC[1] = config.tccCnt / 2;
    TCC0_REGS->TCC_PER = config.tccCnt - 1;

    while (TCC0_REGS->TCC_SYNCBUSY != 0U)
    {
        /* Wait for sync */
    }

    TCC1_PWMInitialize();
    // MCC doesn't expose oneshot; set manually
    TCC1_REGS->TCC_CTRLBSET = TCC_CTRLBSET_ONESHOT(1);

    setPulseCount(10, config);

    PID_DRAIN_EN_Set();

    TCC0_PWMStart();
    TCC1_PWMStart();

    OSCCTRL_REGS->OSCCTRL_INTFLAG = OSCCTRL_INTFLAG_DPLLLCKF_Msk;
    // Fire software event to start both TCCs
    sendSoftwareEventTriggerForTccs();
}

// TODO: Include the shared header for this
typedef struct
{
    uint8_t header2;//PIECE_ID_DATA
    uint8_t square;//also not PIECE_ID_DATA.
    uint16_t new_tones;//bitfield showing which were updated this scan

    //Raw ADC values per rank/file/tone
    uint8_t data[12];
} piece_id_packet_t;

static void onComplete() {
  piece_id_packet_t packet;
  packet.header2 = PIECE_ID_DATA;
  packet.square = activeSpot;
  packet.new_tones = 0x0fff;
  memcpy(packet.data, pieceIdResult, sizeof(packet.data));
  static_assert(sizeof(packet.data) == sizeof(pieceIdResult));

  sendGeneric(PIECE_ID_DATA, sizeof(packet), &packet);
}

bool pieceIdInProgress() {
  return pieceIdRunning;
}

void pieceIdAddPrioritySquares(uint64_t bitboard) {
  priorityScanBitBoard |= bitboard;
  defaultScanBitBoard &= ~bitboard;
}

void pieceIdSetQuiet(bool quiet) {
  isQuiet = quiet;
}

bool pieceIdSetFrequencyParams(unsigned idx, uint32_t frequency, uint16_t drainCounts48MHz, uint16_t ringCounts48MHz) {
  if (idx >= ARRAY_SIZE(frequencies)) {
    return false;
  }

  frequencies[idx].frequency = frequency;
  frequencies[idx].drainDelayCounts = drainCounts48MHz;
  frequencies[idx].ringTimeCounts = ringCounts48MHz;

  calculateFrequencyConfigForIndex(idx);

  return true;
}

void pieceIdSetFrequencyCount(unsigned count) {
  FrequencyCount = count;
}

unsigned pieceIdGetFrequencyCount() {
  return FrequencyCount;
}

uint32_t pieceIdGetFrequency(unsigned idx) {
  if (idx < FrequencyCount) {
    return frequencies[idx].frequency;
  }

  return 0;
}

uint16_t pieceIdGetDelay(unsigned idx) {
  if (idx < FrequencyCount) {
    return frequencies[idx].drainDelayCounts;
  }

  return 0;
}

uint16_t pieceIdGetRing(unsigned idx) {
  if (idx < FrequencyCount) {
    return frequencies[idx].ringTimeCounts;
  }

  return 0;
}

bool pieceIdReady(uint32_t now) {
  // Do pieceId any time we're not doing anything else
  return !pieceIdRunning;
}

void pieceIdTask() {
  if (pieceIdRunning) {
    if (adcDone) {
      adcDone = false;
      pieceIdResult[frequencyIdx] = ADC0_ConversionResultGet();
      frequencyIdx++;

      if (frequencyIdx < FrequencyCount) {
        setPieceId(frequencyIdx);
      }
      else {
        stopPieceId();

        uint32_t doneTicks = getCurrentTimeTicks();
        shuStatsAddSample(Stat_PieceId_SquareTime, doneTicks - pieceIdStartTicks);
        pieceIdReadyTicks = doneTicks;

        pieceIdRunning = false;
        onComplete();
      }
    }
  }
}

void pieceIdStart() {
  pieceIdStartTicks = getCurrentTimeTicks();
  if (pieceIdReadyTicks > 0) {
    shuStatsAddSample(Stat_PieceId_StartLatency, pieceIdStartTicks - pieceIdReadyTicks );
    pieceIdReadyTicks = 0;
  }

  frequencyIdx = 0;
  pieceIdRunning = true;

  activeSpot = chooseScanSquare();

  setupPieceId();
  selectMuxPathForSpot(activeSpot);
  enableMuxes();
  setPieceId(frequencyIdx);
}
