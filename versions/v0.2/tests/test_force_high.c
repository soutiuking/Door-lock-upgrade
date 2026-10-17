#include <assert.h>
#include <stdio.h>
static unsigned char registers[0x6000];
#define REG8(a) registers[(a)]
#define _asm(s) ((void)0)
#include "main_host.c"

int main(void)
{
    /* Use the source's default: constant-high diagnostic must be enabled. */
    assert(FORCE_PC4_HIGH_TEST == 1);
    PA_CR1 = 1;
    hardware_init();
    assert(PC_ODR & 0x10);
    assert(PC_DDR & 0x10);
    assert(PC_CR1 & 0x10);
    assert(PC_CR2 == 0 && PB_CR2 == 0);
    assert(CLK_PCKENR1 == 0 && CLK_PCKENR2 == 0 && CLK_PCKENR3 == 0);
    assert(CLK_CCOR == 0 && PA_CR1 == 1 && PA_DDR == 0);
    PB_IDR = 0;
    EXTI1_IRQHandler();
    TIM4_UPD_IRQHandler();
    assert(PC_ODR & 0x10);
    PB_IDR = 1;
    EXTI1_IRQHandler();
    TIM4_UPD_IRQHandler();
    assert(PC_ODR & 0x10);
    puts("PASS: default constant-high test, push-pull PC4, IRQ/clocks off, debug preserved");
    return 0;
}
