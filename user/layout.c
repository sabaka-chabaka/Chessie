#include "ulib.h"

#define BIG 65536

static const char rodata_str[] = "rodata-ok";
static volatile int data_vals[4] = { 11, 22, 33, 44 };
static volatile uint8_t bss_big[BIG];
static volatile uint32_t bss_small;

int main(void)
{
    int bad = 0;

    for (uint32_t i = 0; i < BIG; i++) {
        if (bss_big[i]) {
            bad++;
            break;
        }
    }
    if (bss_small)                                      bad++;
    if (data_vals[0] != 11 || data_vals[1] != 22 ||
        data_vals[2] != 33 || data_vals[3] != 44)       bad++;
    if (!u_streq(rodata_str, "rodata-ok"))              bad++;

    uint32_t pid = (uint32_t)sys_getpid();
    uint32_t t0 = sys_ticks();
    uint32_t rounds = 0;

    while ((uint32_t)(sys_ticks() - t0) < 15 || rounds < 3) {
        uint8_t pat = (uint8_t)(pid * 37u + rounds * 11u);

        for (uint32_t i = 0; i < BIG; i++)
            bss_big[i] = (uint8_t)(pat ^ (uint8_t)i);
        bss_small = pid * 1000u + rounds;
        data_vals[1] = (int)(pid + rounds);

        for (uint32_t i = 0; i < BIG; i++) {
            if (bss_big[i] != (uint8_t)(pat ^ (uint8_t)i)) {
                bad++;
                break;
            }
        }
        if (bss_small != pid * 1000u + rounds)          bad++;
        if (data_vals[1] != (int)(pid + rounds))        bad++;
        rounds++;
    }

    return bad > 100 ? 100 : bad;
}