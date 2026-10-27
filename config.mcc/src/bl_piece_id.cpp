#include "bl_piece_id.hpp"
#include "bl_piece_id_freq_calc.hpp"
#include "hub_log.h"
#include "peripheral/adc/plib_adc0.h"
#include "peripheral/port/plib_port.h"
//#include "peripheral/tc/plib_tc0.h"
#include "peripheral/tc/plib_tc3.h"
#include "peripheral/tcc/plib_tcc0.h"
#include "peripheral/tcc/plib_tcc1.h"
#include "pic32cm6408jh00048.h"
#include <cstdlib>

// The Clock chain is:
//
// 48MHz OSC -> (Gclkdiv) GCLK8 (dynamic control to get target freq)
// GCLK8     -> PLL  (48Mhz -> 96Mhz)  Div 1
// PLL       -> TCC
// TCC count and period determine the waveform
//
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
//      (Maybe need to consider this for PLL behavior?  Probably not...)_
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
//  TCC ... no notes?
//

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


enum GenClkSrc {
  XOSC        = 0x0,  // XOSC oscillator output
  GCLKIN      = 0x1,  // Generator input pad (GCLK_IO)
  GCLKGEN1    = 0x2,  // Generic clock generator 1 output
  OSCULP32K   = 0x3,  // OSCULP32K oscillator output
  OSC32K      = 0x4,  // OSC32K oscillator output
  XOSC32K     = 0x5,  // XOSC32K oscillator output
  OSC48M      = 0x6,  // OSC48M oscillator output
  FDPLL96M    = 0x7,  // DPLL96M output
};

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

FrequencyData frequencies[] = {
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

static size_t FrequencyCount = ARRAY_SIZE(frequencies);

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

static uint16_t pieceIdResult[ARRAY_SIZE(frequencies)];
static uint8_t frequencyIdx;
static bool pieceIdRunning;
static volatile bool adc_done;

void adcIrq(ADC_STATUS status, uintptr_t context){
    adc_done = true;
}

void disableGclk(uint8_t gclkNum) {
    uint32_t val = GCLK_REGS->GCLK_GENCTRL[gclkNum];
    val &= ~(GCLK_GENCTRL_GENEN_Msk);
    GCLK_REGS->GCLK_GENCTRL[gclkNum] = val;

    // Synchronize disable
    while((GCLK_REGS->GCLK_SYNCBUSY & GCLK_SYNCBUSY_GENCTRL8_Msk) == GCLK_SYNCBUSY_GENCTRL8_Msk) {
    }
}

void setGclkDiv(uint8_t gclkNum, uint8_t divider) {
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

void setPulseCount(uint8_t pulses, const PllTccConfig & config) {
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

void disablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(0);

    while((OSCCTRL_REGS->OSCCTRL_DPLLSYNCBUSY & OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk) == OSCCTRL_DPLLSYNCBUSY_ENABLE_Msk )
    {
        /* Waiting for the DPLL enable synchronization */
    }
}

void enablePll() {
    OSCCTRL_REGS->OSCCTRL_DPLLCTRLA = (uint8_t)(OSCCTRL_DPLLCTRLA_ENABLE_Msk);

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

static void calculateFrequencyConfigs() {
  for (unsigned i = 0; i < FrequencyCount; ++i) {
    frequencies[i].config = getPllTccConfigForFreq(frequencies[i].frequency);
  }
}

// Call once to initialize systems after boot
void initPieceId() {
    calculateFrequencyConfigs();
    initPll();
}

// Call after captouch or any other task which shares resources with PieceId
void setupPieceId() {
  ADC0_CallbackRegister(adcIrq, 0);
  ADC0_ChannelSelect(ADC_POSINPUT_AIN1, ADC_NEGINPUT_GND);
  ADC0_Enable();
  configPinsForPieceId();
}

void stopPieceId() {
  disableMuxes();
  TCC0_PWMStop();
  TCC1_PWMStop();
  TC3_CompareStop();
  ADC0_Disable();
  ADC0_CallbackRegister(nullptr, 0);
}

void setPieceId(unsigned freqIdx) {
    if (freqIdx >= FrequencyCount) {
        return;
    }

    adc_done = false;
    auto & config = frequencies[freqIdx].config;

    // TODO: Could maybe parallelize some of these waits?  Write different systems
    // then wait for sync?
    TCC0_PWMStop();
    TCC1_PWMStop();
    disablePll();
    disableGclk(8);

    setGclkDiv(8, config.gclkDiv);

    const uint32_t whole = config.ldrWhole - 1;
    const uint32_t frac = config.ldrFrac;

    setPllMult(whole, frac);
    enablePll();

    TCC0_PWMInitialize();

    TCC0_REGS->TCC_CC[1] = config.tccCnt / 2;
    TCC0_REGS->TCC_PER = config.tccCnt - 1;

    while (TCC0_REGS->TCC_SYNCBUSY != 0U)
    {
        /* Wait for sync */
    }

    TCC1_PWMInitialize();
    // PWM interface doesn't expose oneshot; set manually
    TCC1_REGS->TCC_CTRLBSET = TCC_CTRLBSET_ONESHOT(1);

    setPulseCount(10, config);
    // Manually set "None" generator event so we can start both TCC0 and TCC1 at the same
    // time with a software event.  MCC doesn't support "None".

    // EVSYS channel 1: software-triggered START for TCC0 + TCC1 (no generator)
    EVSYS_REGS->EVSYS_CHANNEL[1] = EVSYS_CHANNEL_EVGEN(0UL)      // NONE
                                | EVSYS_CHANNEL_PATH(2UL)       // asynchronous (required for TCC users)
                                | EVSYS_CHANNEL_EDGSEL(0UL);    // must be 0 on the async path

    // USER.CHANNEL holds channel number + 1
    EVSYS_REGS->EVSYS_USER[9]  = EVSYS_USER_CHANNEL(2UL);        // TCC0 EV0 <- channel 1
    EVSYS_REGS->EVSYS_USER[15] = EVSYS_USER_CHANNEL(2UL);        // TCC1 EV0 <- channel 1

    uint16_t tc3StartCount = 0x0;

    TC3_REGS->COUNT16.TC_CC[0] = frequencies[freqIdx].drainDelayCounts;
    TC3_REGS->COUNT16.TC_CC[1] = frequencies[freqIdx].ringTimeCounts;

    while((TC3_REGS->COUNT16.TC_SYNCBUSY) != 0U)
    {
        /* Wait for Write Synchronization */
    }

    // Clear PORTB 23 on event
    PORT_REGS->GROUP[1].PORT_EVCTRL = PORT_EVCTRL_PORTEI0_Msk | PORT_EVCTRL_EVACT0(2) | PORT_EVCTRL_PID0(23);

    // Enable does not start counting when an input START event is configured;
    // this timer will not run until triggered by TC0
    TC3_CompareStart();

    PID_DRAIN_EN_Set();

    TCC0_PWMStart();
    TCC1_PWMStart();
    // Fire software event
    EVSYS_REGS->EVSYS_SWEVT = EVSYS_SWEVT_CHANNEL1_Msk;

    // "Retrigger" TCC0 to start after stop
    //TCC0_REGS->TCC_CTRLBSET = TCC_CTRLBSET_CMD(1);
}

#ifdef RANDOM_STEP
static constexpr uint32_t FreqMin = 3'000'000u;
static constexpr uint32_t FreqMax = 10'000'000u;
static constexpr uint32_t FreqStep =  500'000u;

uint32_t freqVal = FreqMin;

uint32_t randVal(uint32_t below) {
  uint32_t val = rand();
  val %= below;
  return val;
}

uint32_t getRandStep() {
  uint32_t val = randVal(FreqStep);
  return val + FreqStep / 2;
}

void switchPieceId() {
  freqVal += getRandStep();
  while (freqVal > FreqMax) {
    freqVal -= (FreqMax - FreqMin);
  }

  hubLog('I', "%u", freqVal);

  setPieceId(freqVal);
}
#else

#if 0
void switchPieceId(uint8_t spot) {
  if (freqIdx >= FrequencyCount) {
    freqIdx = 0;
  }

  selectMuxPathForSpot(spot);
  enableMuxes();

  setPieceId(freqIdx);
  freqIdx++;
}
#endif
#endif

bool pieceIdComplete() {
  return !pieceIdRunning;
}

bool getPieceIdResult(const uint16_t ** ptr, size_t * count) {
  *ptr = pieceIdResult;
  *count = FrequencyCount;

  // Todo:  valid?
  return true;
}

void pieceIdTask() {
  if (pieceIdRunning) {
    if (adc_done) {
      adc_done = false;
      pieceIdResult[frequencyIdx] = ADC0_ConversionResultGet();
      frequencyIdx++;

      //PID_DRAIN_EN_Set();
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      asm volatile ("nop");
      //PID_DRAIN_EN_Clear();

      if (frequencyIdx < FrequencyCount) {
        setPieceId(frequencyIdx);
      }
      else {
        stopPieceId();
        pieceIdRunning = false;
      }
    }
  }
}

void startPieceId(uint8_t spot) {
  frequencyIdx = 0;
  pieceIdRunning = true;

  selectMuxPathForSpot(spot);
  enableMuxes();

  setPieceId(frequencyIdx);
}
