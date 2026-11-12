/* TEST-ONLY. A separate translation unit that includes the Field Manual's loop-gain header, so a test can compare its
 * COSTAS_KP_Q16 / COSTAS_KI_Q16 with the Handbook's (hb_loop_gains.h) without the two macro sets colliding. */
#include "../../../field-manual/include/fm_loops.h"
int32_t fm_costas_kp(double kp, double kd) { return COSTAS_KP_Q16(kp, kd); }
int32_t fm_costas_ki(double ki, double kd, double sps) { return COSTAS_KI_Q16(ki, kd, sps); }
/* the worked-example values both books print, from the Field Manual's own call sites */
const int32_t fm_printed_kp = COSTAS_KP_Q16(0.05192, 11200.0);
const int32_t fm_printed_ki = COSTAS_KI_Q16(0.001385, 11200.0, 2);
