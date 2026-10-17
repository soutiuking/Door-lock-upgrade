/* run_tests.ps1 removes Cosmic ISR/asm syntax; register effects are simulated. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char registers[0x6000];
#define REG8(a) registers[(a)]
#define _asm(s) ((void)0)
#define FORCE_PC4_HIGH_TEST 0
#include "main_host.c"

static void ticks(unsigned int ms)
{
    while (ms--) TIM4_UPD_IRQHandler();
}

int main(void)
{
    unsigned int i;
    memset(registers, 0, sizeof(registers));
    PA_CR1 = 1;
    PB_IDR = 1;
    hardware_init();
    assert(boot_pending && timer_running && !(PC_ODR & 0x10));
    assert(PB_CR2 == 0 && PC_CR2 == 0 && EXTI_CR1 == 4);
    assert((PA_CR1 & 3) == 1 && (PA_DDR & 3) == 0 && (PA_CR2 & 3) == 0);
    ticks(49);
    assert(boot_pending && !(PC_ODR & 0x10));
    /* Change just before the single read: only the level at 50 ms counts. */
    PB_IDR = 0;
    ticks(1);
    assert(!boot_pending && control.state == KNOCK_WAIT);
    assert(!timer_running && (PC_CR2 & 2) && !(PC_ODR & 0x10));
    puts("PASS: 50 ms startup wait, single sample, IRQ setup and debug preservation");

    PB_IDR = 1;                    /* Changes after sampling have no effect. */
    apply_control();
    assert(!timer_running && control.state == KNOCK_WAIT);
    EXTI1_IRQHandler();
    assert(timer_running && TIM4_PSCR == 4 && TIM4_ARR == 124);
    ticks(1000);
    EXTI1_IRQHandler();
    ticks(1000);
    EXTI1_IRQHandler();
    assert(control.state == KNOCK_POWER && (PC_ODR & 0x10));
    for (i = 0; i < 59999; ++i) {
        PB_IDR = (unsigned char)(i & 1);
        ticks(1);
        assert((PC_ODR & 0x10) && (PC_DDR & 0x10) && (PC_CR1 & 0x10));
        assert(control.state == KNOCK_POWER && PB_CR2 == 0);
    }
    ticks(1);
    assert(control.state == KNOCK_WAIT && !(PC_ODR & 0x10));
    assert(!timer_running && CLK_PCKENR1 == 0 && TIM4_IER == 0);
    puts("PASS: latched use mode; PB0 changes cannot interrupt or extend 60 s power");

    EXTI1_IRQHandler();
    ticks(10001);
    assert(!timer_running && control.state == KNOCK_WAIT && (PC_CR2 & 2));
    puts("PASS: failed sequence returns to HALT-ready state");

    /* Simulated reset re-enters hardware_init and resamples only after 50 ms. */
    PB_IDR = 1;
    hardware_init();
    assert(!(PC_ODR & 0x10) && boot_pending);
    ticks(50);
    assert(control.state == KNOCK_TEST && (PC_ODR & 0x10));
    assert(!timer_running && CLK_PCKENR1 == 0 && PB_CR2 == 0);
    PB_IDR = 0;
    for (i = 0; i < 1000; ++i) {
        apply_control();
        assert((PC_ODR & 0x10) && !timer_running && control.state == KNOCK_TEST);
    }
    assert((PC_DDR & 0x10) && (PC_CR1 & 0x10) && !(PC_CR2 & 2));
    puts("PASS: latched test mode; PB0 low cannot remove static power; timer off");

    hardware_init();
    ticks(49);
    assert(boot_pending && !(PC_ODR & 0x10));
    ticks(1);
    assert(!boot_pending && control.state == KNOCK_WAIT && !timer_running);
    puts("PASS: mode can change only after reset and new 50 ms startup wait");
    return 0;
}
