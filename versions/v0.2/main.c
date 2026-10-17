/* STM8L051F3, Cosmic STM8 C compiler. No SPL dependency. */
#include "stm8l051_regs.h"
/* TEMPORARY DIAGNOSTIC: set to 0 to restore normal knock/boot-mode logic. */
#ifndef FORCE_PC4_HIGH_TEST
#define FORCE_PC4_HIGH_TEST 0
#endif

#if !FORCE_PC4_HIGH_TEST
#include "knock_control.h"

static KnockControl control;
static volatile unsigned char timer_running;
#define BOOT_SETTLE_MS 50U
static unsigned char boot_pending;
static unsigned char boot_ms;
static unsigned char power_enabled;

static void timer_start(void)
{
    if (timer_running) return;
    CLK_PCKENR1 = 0x04;             /* TIM4 only */
    TIM4_CR1 = 0;
    TIM4_PSCR = 4;                  /* 2 MHz / 16 / 125 = 1 kHz */
    TIM4_ARR = 124;
    TIM4_CNTR = 0;
    TIM4_EGR = 1;
    TIM4_SR1 = 0;
    TIM4_IER = 1;
    timer_running = 1;
    TIM4_CR1 = 1;
}

static void apply_control(void)
{
    unsigned char want_power =
        control.state == KNOCK_POWER || control.state == KNOCK_TEST;
    /* PC4 is a static push-pull enable, never PWM. Leave its latch untouched
       between transitions, including TIM4 / EXTI service and WFI. */
    if (want_power != power_enabled) {
        if (want_power) PC_ODR |= 0x10;
        else PC_ODR &= (unsigned char)~0x10;
        power_enabled = want_power;
    }

    if (!boot_pending && (control.state == KNOCK_WAIT ||
        (control.state == KNOCK_COLLECT && control.armed))) {
        /* Clear stale edges ONLY when re-enabling an inhibited input. */
        if (!(PC_CR2 & 0x02)) {
            EXTI_SR1 = 0x02;
            PC_CR2 |= 0x02;
        }
    } else {
        PC_CR2 &= (unsigned char)~0x02;
    }

    if (control.state == KNOCK_COLLECT || control.state == KNOCK_POWER ||
        boot_pending) {
        timer_start();
    } else {
        TIM4_IER = 0;
        TIM4_CR1 = 0;
        TIM4_SR1 = 0;
        CLK_PCKENR1 = 0;
        timer_running = 0;
    }
}

@far @interrupt void EXTI1_IRQHandler(void)
{
    EXTI_SR1 = 0x02;                /* PC1: rising edge */
    /* The pending flag captures even a pulse ending before ISR entry. */
    if (!boot_pending) knock_edge(&control);
    apply_control();
}

@far @interrupt void TIM4_UPD_IRQHandler(void)
{
    TIM4_SR1 = 0;
    if (boot_pending) {
        if (++boot_ms >= BOOT_SETTLE_MS) {
            /* The ONLY PB0 read: latch until reset, never enable PB0 IRQ. */
            boot_pending = 0;
            knock_init(&control, (PB_IDR & 0x01) != 0);
        }
    } else {
        knock_tick(&control, (PC_IDR & 0x02) != 0);
    }
    apply_control();
}

static void hardware_init(void)
{
    /* Reset default source is HSI 16 MHz; divide by 8 for 2 MHz. */
    CLK_CKDIVR = 3;
    CLK_PCKENR1 = 0;
    CLK_PCKENR2 = 0;                /* ADC, RTC, DMA, COMP, boot-ROM clock off */
    CLK_PCKENR3 = 0;
    CLK_CRTCR = 0;                  /* No RTC clock: use HALT, not active HALT */
    CLK_CCOR = 0;
    CLK_CBEEPR = 0;
    CLK_ECKCR = 0;
    CLK_ICKCR = 0x01;               /* HSI only, no LSI / fast wake-up */
    PWR_CSR1 = 0;                   /* PVD off */
    PWR_CSR2 = 0x02;                /* ULP: internal reference off in HALT */

    /* Leave PA0/SWIM and PA1/NRST exactly as reset. Unused pins: pull-up
       inputs, no IRQ. Adapt masks if the board connects another signal. */
    PA_DDR &= 0x03;
    PA_CR1 |= 0x0C;                 /* Bonded PA2/PA3 only */
    PA_CR2 &= 0x03;
    PB_DDR = 0;
    PB_CR1 = 0xFE;                  /* PB0 external H/L strap REQUIRED */
    PB_CR2 = 0;
    PC_ODR = 0;                     /* PC4 off before enabling output */
    power_enabled = 0;
    PC_DDR = 0x11;                  /* Unused PC0: true open-drain, drive low */
    PC_CR1 = 0x70;                  /* PC4 push-pull, PC5/6 pull-ups */
    PC_CR2 = 0;
    PD_DDR = 0;
    PD_CR1 = 0x01;                  /* Bonded PD0 only */
    PD_CR2 = 0;

    EXTI_CONF1 = 0;                 /* PC1 uses pin interrupt */
    EXTI_CR1 = 0x04;                /* pin 1 rising only; PB0 IRQ disabled */
    EXTI_SR1 = 0xFF;                /* W1C */
    EXTI_SR2 = 0xFF;
    PB_CR2 = 0;

    timer_running = 0;
    boot_pending = 1;
    boot_ms = 0;
    knock_init(&control, 0);        /* PC4 stays low during 50 ms settling */
    apply_control();
}

#else
/* Minimal constant-high diagnostic: no PB0/PC1 logic, timer, or sleep.
   SWIM/NRST remain untouched. All peripheral clocks and GPIO IRQs are off. */
static void hardware_init(void)
{
    /* Preload HIGH before enabling push-pull output: no firmware low pulse. */
    PC_ODR |= 0x10;
    PC_CR1 |= 0x10;
    PC_DDR |= 0x10;
    PC_CR2 = 0;
    PB_CR2 = 0;
    PA_CR2 &= 0x03;
    PD_CR2 = 0;
    CLK_CKDIVR = 3;
    CLK_PCKENR1 = 0;
    CLK_PCKENR2 = 0;
    CLK_PCKENR3 = 0;
    CLK_CCOR = 0;                  /* CCO must not take over PC4 */
}

/* Keep the existing vector table linkable; all IRQ sources remain masked. */
@far @interrupt void EXTI1_IRQHandler(void)
{
    EXTI_SR1 = 0x02;
}

@far @interrupt void TIM4_UPD_IRQHandler(void)
{
    TIM4_SR1 = 0;
}
#endif

void main(void)
{
    _asm("sim");
    hardware_init();
#if FORCE_PC4_HIGH_TEST
    for (;;) {
        _asm("nop");               /* RUN only; PC4 latch remains HIGH */
    }
#else
    for (;;) {
        /* HALT/WFI enable interrupts themselves on STM8. Keep SIM adjacent
           to the decision: no ISR can run between checking and sleeping.
           Pending EXTI is NOT cleared here, so a late edge is not lost. */
        _asm("sim");
        if (timer_running) {
            _asm("wfi");           /* TIM4 runs while CPU waits */
        } else {
            _asm("halt");          /* PC1 wake; GPIO output retained */
        }
        _asm("rim");
    }
#endif
}
