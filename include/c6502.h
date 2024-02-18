#ifndef C6502_H
#define C6502_H

#include <stdint.h>
#include <stdbool.h>
#include "ccbBus.h"

#define BOOT_SUCCESS 0

typedef struct registers
{
    uint16_t pc;
    uint8_t sp;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    // N V - B D I Z C
    uint8_t flags;
} cpu_state;

typedef enum FLAGS6502
{
    C = (1 << 0), // Carry bit
    Z = (1 << 1), // Zero
    I = (1 << 2), // Disable interrupts
    D = (1 << 3), // Decimal mode ()
    B = (1 << 4), // Break
    U = (1 << 5), // Unused
    V = (1 << 6), // Overflow
    N = (1 << 7)  // Negative
} FLAGS6502;

void initialize_c6502(ccb_bus* pBus);

// ideas from OLC
void cpu_clock();
void reset();
void irq();
void nmi();
bool complete();
// end ideas from OLC

const cpu_state* get_cpu_state();

#endif
