/* Startup for the cycle bench on the QEMU machine mps2-an386 (Cortex-M4 with FPU). */
#include <stdint.h>
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int main(void);
void Reset_Handler(void);
static void Default_Handler(void) { for (;;) { } }
__attribute__((section(".vectors"), used))
const void *vectors[16] = { &_estack, Reset_Handler, Default_Handler, Default_Handler, Default_Handler,
                            Default_Handler, Default_Handler, 0, 0, 0, 0, Default_Handler,
                            Default_Handler, 0, Default_Handler, Default_Handler };
extern void __libc_init_array(void);
void _init(void) { }
void _fini(void) { }
extern void exit(int);
void Reset_Handler(void)
{
    uint32_t *s = &_sidata, *d = &_sdata;
    while (d < &_edata) *d++ = *s++;
    for (d = &_sbss; d < &_ebss; ) *d++ = 0;
    *(volatile uint32_t *)0xE000ED88u |= 0xFu << 20;   /* CPACR: full access to CP10 and CP11, the FPU */
    __asm volatile("dsb\n isb");
    __libc_init_array();
    exit(main());
}
