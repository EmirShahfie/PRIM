// bradv1.c
// BRAD-v1 front-end timing attack prototype for BOOM
// Summarizes results per secret bit instead of printing all rounds.

#include <stdio.h>
#include <stdint.h>
#include "rlibsc.h"

// ------------------------------------------------------------
// Configuration
// ------------------------------------------------------------

#define SECRET_DATA_LEN   8     // number of secret bits
#define ATTACK_ROUNDS     500    // increase for cleaner distributions

// ------------------------------------------------------------
// Global data
// ------------------------------------------------------------

// Secret-controlled branches (victim)
static volatile uint8_t sec_data[SECRET_DATA_LEN] = {
    1,0,0,1,
    0,1,1,0,
};

// Attacker array: index 0 → value used for spy_f(0)
//                 index 1 → value used for spy_f(1)
static volatile uint8_t array1[2] = { 0, 1 };

// ------------------------------------------------------------
// Conditional branch gadget
// ------------------------------------------------------------

__attribute__((noinline))
static void condBranch(volatile uint8_t *addr)
{
    if (*addr) {
        asm volatile("nop");
        asm volatile("nop");
    } else {
        asm volatile("addi t1, x0, 2");
    }
}

__attribute__((noinline))
static void victim_f(uint8_t idx)
{
    condBranch(&sec_data[idx]);
}

__attribute__((noinline))
static void spy_f(uint8_t idx)
{
    condBranch(&array1[idx]);
}

// ------------------------------------------------------------
// main
// ------------------------------------------------------------

int main(void)
{
    printf("BRAD-v1 timing test starting…\n");
    printf("SECRET_DATA_LEN=%d, ATTACK_ROUNDS=%d\n\n",
           SECRET_DATA_LEN, ATTACK_ROUNDS);

    // For storing final guessed secret bits (optional)
    uint8_t guessed_secret[SECRET_DATA_LEN];

    // Loop over each secret bit
    for (int i = 0; i < SECRET_DATA_LEN; i++) {

        uint64_t sum0 = 0;   // cumulative cycles for spy_f(0)
        uint64_t sum1 = 0;   // cumulative cycles for spy_f(1)

        for (int k = 0; k < ATTACK_ROUNDS; k++) {
            // 1) Train predictor by calling victim twice
            victim_f(i);
            victim_f(i);
            fence();

            // 2) Time spy_f(0)
            uint64_t start0 = rdcycle();
            spy_f(0);
            uint64_t end0 = rdcycle();
            sum0 += (end0 - start0);

            // 3) Time spy_f(1)
            uint64_t start1 = rdcycle();
            spy_f(1);
            uint64_t end1 = rdcycle();
            sum1 += (end1 - start1);
        }

        double avg0 = (double)sum0 / (double)ATTACK_ROUNDS;
        double avg1 = (double)sum1 / (double)ATTACK_ROUNDS;

        // Heuristic:
        //  - If secret bit == 1, predictor is trained on "taken" (addr=1),
        //    so spy_f(0) (array1[0] = 1) should align with prediction → faster.
        //  - If secret bit == 0, spy_f(1) should be faster.
        //
        // So:
        //   avg0 < avg1  → guess secret bit = 1
        //   avg1 <= avg0 → guess secret bit = 0
        uint8_t guess_bit = (avg0 < avg1) ? 0 : 1;
        guessed_secret[i] = guess_bit;

        printf("Index %2d | true=%u | avg(spy0)=%d cycles | avg(spy1)=%d cycles | ",
               i, (unsigned)sec_data[i], (int)avg0, (int)avg1);

        if (avg0 < avg1) {
            printf("faster=spy(0) → guess=0\n");
        } else {
            printf("faster=spy(1) → guess=1\n");
        }
    }

    // Optional: print reconstructed secret bitstring at the end
    printf("\nGuessed secret bits: ");
    for (int i = 0; i < SECRET_DATA_LEN; i++) {
        printf("%u", (unsigned)guessed_secret[i]);
    }
    printf("\nTrue    secret bits: ");
    for (int i = 0; i < SECRET_DATA_LEN; i++) {
        printf("%u", (unsigned)sec_data[i]);
    }
    printf("\n");

    return 0;
}