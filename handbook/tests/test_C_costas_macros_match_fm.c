/* The Handbook and the Field Manual both define COSTAS_KP_Q16 and COSTAS_KI_Q16. They must take the same arguments
 * (kd = detector counts per radian, not the amplitude) and return the same integers, so a call copied from one book
 * into the other keeps its meaning. fm_costas_tu.c is the Field Manual's header in its own translation unit. */
#include <stdint.h>
#include "check.h"
#include "hb_loop_gains.h"

int32_t fm_costas_kp(double kp, double kd);
int32_t fm_costas_ki(double ki, double kd, double sps);
extern const int32_t fm_printed_kp, fm_printed_ki;

int main(void) {
    int n = 0, bad = 0;
    const double kds[] = { 1100.0, 3000.0, 11200.0, 15600.0, 20000.0, 65000.0 };
    const double kps[] = { 0.05192, 0.0519, 0.0100, 0.2 };
    const double kis[] = { 0.001385, 0.00138, 0.0001 };
    const double spss[] = { 1.0, 2.0, 4.0, 8.0 };
    for (unsigned a = 0; a < sizeof kds / sizeof *kds; a++) {
        for (unsigned b = 0; b < sizeof kps / sizeof *kps; b++) {
            n++; if (COSTAS_KP_Q16(kps[b], kds[a]) != fm_costas_kp(kps[b], kds[a])) bad++;
        }
        for (unsigned b = 0; b < sizeof kis / sizeof *kis; b++)
            for (unsigned c = 0; c < sizeof spss / sizeof *spss; c++) {
                n++; if (COSTAS_KI_Q16(kis[b], kds[a], spss[c]) != fm_costas_ki(kis[b], kds[a], spss[c])) bad++;
            }
    }
    CHECK(bad == 0, "%d of %d (kp, ki, kd, sps) combinations differ between the books' macros", bad, n);
    /* the Field Manual's printed worked example, reproduced through the Handbook's macro (Kd = 2 * 5600) */
    CHECK(COSTAS_KP_Q16(0.05192, 11200.0) == 207671349 && fm_printed_kp == 207671349, "Kp worked example %d", COSTAS_KP_Q16(0.05192, 11200.0));
    CHECK(COSTAS_KI_Q16(0.001385, 11200.0, 2) == 2769885 && fm_printed_ki == 2769885, "Ki worked example %d", COSTAS_KI_Q16(0.001385, 11200.0, 2));
    /* the Handbook's own worked example: A = 7800, so Kd = 15600; same values the chapter prints (about 1.49e8 and 1.99e6) */
    CHECK(COSTAS_KP_Q16(0.05192, 2.0 * 7800.0) > 1.48e8 && COSTAS_KP_Q16(0.05192, 2.0 * 7800.0) < 1.50e8, "Handbook Kp %d", COSTAS_KP_Q16(0.05192, 2.0 * 7800.0));
    CHECK(COSTAS_KI_Q16(0.001385, 2.0 * 7800.0, 2) > 1.98e6 && COSTAS_KI_Q16(0.001385, 2.0 * 7800.0, 2) < 2.00e6, "Handbook Ki %d", COSTAS_KI_Q16(0.001385, 2.0 * 7800.0, 2));
    printf("  [info] %d combinations compared; Handbook A = 7800 gives kp %d, ki %d\n", n, COSTAS_KP_Q16(0.05192, 2.0 * 7800.0), COSTAS_KI_Q16(0.001385, 2.0 * 7800.0, 2));
    TEST_DONE("test_C_costas_macros_match_fm");
}
