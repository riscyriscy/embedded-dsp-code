/* TEST-ONLY. HB[79]: the two worked-example gain statements, #included inside a test function (lrintf is not a
 * constant expression, so they cannot sit at file scope). */
int32_t kp_q16 = COSTAS_KP_Q16(0.05192, 15600.0);     // about 1.49e8
int32_t ki_q16 = COSTAS_KI_Q16(0.001385, 15600.0, 2); // about 1.99e6
