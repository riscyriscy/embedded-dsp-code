/* TEST-ONLY stub: the HAL type and the CMSIS interrupt-mask intrinsics the ping-pong DMA listing uses, so it compiles on a host.
 * __disable_irq() / __set_PRIMASK() call optional hooks so a test can model an interrupt that becomes pending while the main loop
 * has interrupts masked (it is then delivered when the mask is released, as on a Cortex-M). */
#ifndef HBA_STM32_HAL_STUB_H
#define HBA_STM32_HAL_STUB_H
#include <stdint.h>
typedef struct { int unused; } ADC_HandleTypeDef;

static uint32_t hbA_primask;
static int hbA_irq_mask_calls;
static void (*hbA_on_irq_masked)(void);
static void (*hbA_on_irq_unmasked)(void);
static inline uint32_t __get_PRIMASK(void) { return hbA_primask; }
static inline void __disable_irq(void) { hbA_primask = 1; hbA_irq_mask_calls++; if (hbA_on_irq_masked) hbA_on_irq_masked(); }
static inline void __set_PRIMASK(uint32_t m) { hbA_primask = m; if (!m && hbA_on_irq_unmasked) hbA_on_irq_unmasked(); }
#endif
