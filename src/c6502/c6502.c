#include <stdint.h>
#include <stdlib.h>
#include <stdio.h> // testing only remove
#include <string.h>
#include <stdbool.h>
#include "c6502.h"
#include "ccbBus.h"
#include "defs.h"

/**************************************************************************
 * File Declarations
 */

ccb_bus* pccb_bus = NULL;

cpu_state regs;

typedef struct operation_struct
{
    char op_name[4];
    uint8_t (*operation)();
    uint8_t (*addr_mode)();
    uint8_t cycles;
} cpu_operations;

cpu_operations op_codes[0x100];     // setup 256 op_code locations
uint8_t cycles = 0;
uint8_t fetched = 0;
uint16_t addr_abs = 0;
uint16_t addr_rel = 0;
uint8_t opcode = 0;
uint16_t abs_addr = 0;


/*****************************************************************************************
 * Forward Declarations of Functions
 *
 */
static uint8_t UNK();
static void write(uint16_t addr, uint8_t value);
static uint8_t read(uint16_t addr);
static uint8_t get_flag(FLAGS6502 f);
static void set_flag(FLAGS6502 f, bool v);

/*****************************************************************************************
 * section to setup all the addressing modes
 *
 *
 *
 */

static uint8_t ACC()
{
    fetched = regs.a;
    return 0;
}

static uint8_t IMM()
{
    addr_abs = regs.pc++;
    return 0;
}

static uint8_t ZP0()
{
    addr_abs = read(regs.pc);
    regs.pc++;
    addr_abs &= 0x00FF;
    return 0;
}

static uint8_t ZPX()
{
    addr_abs = (read(regs.pc) + regs.x);
    regs.pc++;
    addr_abs &= 0x00FF;
    return 0;
}

static uint8_t ZPY()
{
    addr_abs = (read(regs.pc) + regs.y);
    regs.pc++;
    addr_abs &= 0x00FF;
    return 0;
}

static uint8_t ABS()
{
    uint16_t lo = read(regs.pc);
    regs.pc++;
    uint16_t hi = read(regs.pc);
    regs.pc++;

    addr_abs = (hi << 8) | lo;
    return 0;
}

static uint8_t ABX()
{
    uint16_t lo = read(regs.pc);
    regs.pc++;
    uint16_t hi = read(regs.pc);
    regs.pc++;

    addr_abs = (hi << 8) | lo;
    addr_abs += regs.x;

    if ((addr_abs & 0xFF00) != (hi << 8))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static uint8_t ABY()
{
    uint16_t lo = read(regs.pc);
    regs.pc++;
    uint16_t hi = read(regs.pc);
    regs.pc++;

    addr_abs = (hi << 8) | lo;
    addr_abs += regs.y;

    if ((addr_abs & 0xFF00) != (hi << 8))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static uint8_t IDX()
{
    uint16_t t = read(regs.pc);
    regs.pc++;

    uint16_t lo = read((uint16_t)(t + (uint16_t)regs.x) & 0x00FF);
    uint16_t hi = read((uint16_t)(t + (uint16_t)regs.x + 1) & 0x00FF);

    addr_abs = (hi << 8) | lo;

    return 0;
}

static uint8_t IDY()
{
    uint16_t t = read(regs.pc);
    regs.pc++;

    uint16_t lo = read(t & 0x00FF);
    uint16_t hi = read((t + 1) & 0x00FF);

    addr_abs = (hi << 8) | lo;
    addr_abs += regs.y;

    if ((addr_abs & 0xFF00) != (hi << 8))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static uint8_t REL()
{
    addr_rel = read(regs.pc);
    regs.pc++;
    if (addr_rel & 0x80)
    {
        addr_rel |= 0xFF00;
    }

    return 0;
}

static uint8_t IND()
{
    uint16_t ptr_lo = read(regs.pc);
    regs.pc++;
    uint16_t ptr_hi = read(regs.pc);
    regs.pc++;

    uint16_t ptr = (ptr_hi << 8) | ptr_lo;

    if (ptr_lo == 0x00FF) // Simulate page boundary bug
    {
        addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr + 0);
    }
    else // Behave normally
    {
        addr_abs = (read(ptr + 1) << 8) | read(ptr + 0);
    }

    return 0;
}

static uint8_t IMP()
{
    fetched = regs.a;
    return 0;
}

static uint8_t fetch()
{
    if (!(op_codes[opcode].addr_mode == &IMP) && !(op_codes[opcode].addr_mode == &ACC))
    {
        fetched = read(addr_abs);
    }
    return fetched;
}

/*****************************************************************************************
 * Section contain the calls to the processor op codes
 * "unk" is the default. It will just do nothing instead of "illegal" things.
 *
 *
 */
static uint8_t UNK()
{
    printf("Unknown called\n");

    return 0;
}

static uint8_t ADC()
{
    fetch();
    uint16_t temp = (uint16_t)regs.a + (uint16_t)fetched + (uint16_t)get_flag(C);

    set_flag(C, temp > 255);
    set_flag(Z, (temp & 0x00FF) == 0);
    set_flag(N, temp & 0x80);
    set_flag(V, (~((uint16_t)regs.a ^ (uint16_t)fetched) & ((uint16_t)regs.a ^ (uint16_t)temp)) & 0x0080);

    regs.a = temp & 0x00FF;
    return 1;
}

static uint8_t AND()
{
    fetch();
    regs.a = regs.a & fetched;
    set_flag(Z, regs.a == 0x00);
    set_flag(N, regs.a & 0x80);
    return 1;
}

static uint8_t ASL()
{
    fetch();
    uint16_t temp = (uint16_t)fetched << 1;
    set_flag(C, temp & 0x100);
    set_flag(Z, (temp & 0x00FF) == 0x00);
    set_flag(N, temp & 0x80);
    if (op_codes[opcode].addr_mode == &ACC)
    {
        regs.a = (uint8_t)(temp & 0x00FF);
    }
    else
    {
        write(addr_abs, temp & 0x00FF);
    }
    return 0;
}

static uint8_t BCC()
{
    if (get_flag(C) == 0)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BCS()
{
    if (get_flag(C) == 1)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BEQ()
{
    if (get_flag(Z) == 1)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BIT()
{
    fetch();
    uint16_t temp = regs.a & fetched;
    set_flag(Z, (temp & 0x00FF) == 0x00);
    set_flag(N, fetched & (1 << 7)); 
    set_flag(V, fetched & (1 << 6));
    return 0;
}

static uint8_t BMI()
{
    if (get_flag(N) == 1)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BNE()
{
    if (get_flag(Z) == 0)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BPL()
{
    if (get_flag(N) == 0)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BRK()
{
    regs.pc++;

    set_flag(I, 1);
    write(0x0100 + regs.sp, (regs.pc >> 8) & 0x00FF);   // write high byte to stack
    regs.sp++;
    write(0x0100 + regs.sp, regs.pc & 0x00FF);          // write lo byete to stack
    regs.sp++;

    set_flag(B, 1);
    write(0x0100 + regs.sp, regs.flags);
    regs.sp++;
    set_flag(B, 0);

    regs.pc = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
    return 0;
}

static uint8_t BVC()
{
    if (get_flag(V) == 0)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t BVS()
{
    if (get_flag(V) == 1)
    {
        cycles++;
        addr_abs = regs.pc + addr_rel;

        if ((addr_abs & 0xFF00) != (regs.pc & 0xFF00))
        {
            cycles++;
        }

        regs.pc = addr_abs;
    }
    return 0;
}

static uint8_t CLC()
{
    set_flag(C, false);
    return 0;
}

static uint8_t CLD()
{
    set_flag(D, false);
    return 0;
}

static uint8_t CLI()
{
    set_flag(I, false);
    return 0;
}

static uint8_t CLV()
{
    set_flag(V, false);
    return 0;
}

static uint8_t CMP()
{
    fetch();
    uint16_t temp = (uint16_t)regs.a - (uint16_t)fetched;
    set_flag(C, regs.a >= fetched);
    set_flag(Z, temp & 0x00FF == 0x0000);
    set_flag(N, temp & 0x0080);
    return 1;
}

static uint8_t CPX()
{
    fetch();
    uint16_t temp = (uint16_t)regs.x - (uint16_t)fetched;
    set_flag(C, regs.x >= fetched);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    return 0;
}

static uint8_t CPY()
{
    fetch();
    uint16_t temp = (uint16_t)regs.y - (uint16_t)fetched;
    set_flag(C, regs.y >= fetched);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);    
    return 0;
}

static uint8_t DEC()
{
    fetch();
    uint8_t temp = fetched - 1;
    set_flag(Z, temp == 0x00);
    set_flag(N, temp & 0x80);
    write(addr_abs, temp);

    return 0;
}

static uint8_t DEX()
{
    regs.x--;
    set_flag(Z, regs.x == 0x00);
    set_flag(N, regs.x & 0x80);
    return 0;
}

static uint8_t DEY()
{
    regs.y--;
    set_flag(Z, regs.y == 0x00);
    set_flag(N, regs.y & 0x80);
    return 0;
}

static uint8_t EOR()
{
    fetch();
    regs.a ^= fetched;
    set_flag(Z, regs.a == 0x00);
    set_flag(N, regs.a & 0x80);
    return 1;
}

static uint8_t INC()
{
    fetch();
    fetched++;
    set_flag(Z, fetched == 0x00);
    set_flag(N, fetched & 0x80);
    write(addr_abs, fetched);
    return 0;
}

static uint8_t INX()
{
    regs.x++;
    set_flag(Z, regs.x == 0x00);
    set_flag(N, regs.x & 0x80);

    return 0;
}

static uint8_t INY()
{
    regs.y++;
    set_flag(Z, regs.y == 0x00);
    set_flag(N, regs.y & 0x80);
    return 0;
}

static uint8_t JMP()
{
    regs.pc = addr_abs;
    return 0;
}

static uint8_t JSR()
{
    regs.pc--;

    write(0x0100 + regs.sp, (regs.pc >> 8) & 0x00FF);
    regs.sp++;
    write(0x0100 + regs.sp, regs.pc & 0x00FF);
    regs.sp++;

    regs.pc = addr_abs;
    return 0;
}

static uint8_t LDA()
{
    fetch();
    regs.a = fetched;

    // adjust the flags as appropriate
    set_flag(Z, regs.a == 0);
    set_flag(N, regs.a & 0x80);

    return 1;
}

static uint8_t LDX()
{
    fetch();
    regs.x = fetched;

    // adjust the flags as appropriate
    set_flag(Z, regs.x == 0);
    set_flag(N, regs.x & 0x80);
    return 1;
}

static uint8_t LDY()
{
    fetch();
    regs.y = fetched;

    // adjust the flags as appropriate
    set_flag(Z, regs.y == 0);
    set_flag(N, regs.y & 0x80);
    return 1;
}

static uint8_t LSR()
{
    fetch();
    set_flag(C, fetched & 0x0001);
    uint16_t temp = fetched >> 1;
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    if (op_codes[opcode].addr_mode == &ACC)
    {
        regs.a = temp & 0x00FF;
    }
    else
    {
        write(addr_abs, temp & 0x00FF);
    }
    return 0;
}

static uint8_t NOP()
{
    return 0;
}

static uint8_t ORA()
{
    fetch();
    regs.a |= fetched;
    set_flag(Z, regs.a == 0x00);
    set_flag(N, regs.a & 0x80);
    return 1;
}

static uint8_t PHA()
{
    write(0x0100 + regs.sp, regs.a);
    regs.sp--;
    return 0;
}

static uint8_t PHP()
{
    write(0x0100 + regs.sp, regs.flags);
    regs.sp--;
    return 0;
}

static uint8_t PLA()
{
    regs.sp++;
    regs.a = read(0x0100 + regs.sp);
    set_flag(Z, regs.a == 0);
    set_flag(N, regs.a & 0x80);
    return 0;
}

static uint8_t PLP()
{
    regs.sp++;
    regs.flags = read(0x0100 + regs.sp);
    return 0;
}

static uint8_t ROL()
{
    fetch();
    uint16_t temp = (uint16_t)(fetched << 1) | get_flag(C);
    set_flag(C, temp & 0xFF00);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    if (op_codes[opcode].addr_mode == &ACC)
    {
        regs.a = temp & 0x00FF;
    }
    else 
    {
        write(addr_abs, temp & 0x00FF);
    }
    return 0;
}

static uint8_t ROR()
{
    fetch();
    uint16_t temp = (uint16_t)(fetched >> 1) | (get_flag(C) << 7);
    set_flag(C, fetched & 0x01);
    set_flag(Z, (temp & 0x00FF) == 0x0000);
    set_flag(N, temp & 0x0080);
    if (op_codes[opcode].addr_mode == &ACC)
    {
        regs.a = temp & 0x00FF;
    }
    else 
    {
        write(addr_abs, temp & 0x00FF);
    }    
    return 0;
}

static uint8_t RTI()
{
    regs.sp++;
    regs.flags = read(0x0100 + regs.sp);
    regs.flags &= ~B;
    regs.flags &= ~U;

    regs.sp++;
    regs.pc = (uint16_t)read(0x0100 + regs.sp);
    regs.sp++;
    regs.pc |= (uint16_t)read(0x0100 + regs.sp) << 8;
    return 0;
}

static uint8_t RTS()
{
    regs.sp++;
    regs.pc = (uint16_t)read(0x0100 + regs.sp);
    regs.sp++;
    regs.pc |= (uint16_t)read(0x0100 + regs.sp) << 8;
    return 0;
}

static uint8_t SBC()
{
    // A = A - M - (1 - C)
    // A = A + -1(M-(1-C))
    // A = A + -M + 1 + C
    fetch();

    uint16_t value = ((uint16_t)fetched) ^ 0x00FF;

    uint16_t temp = (uint16_t)regs.a + value + (uint16_t)get_flag(C);
    set_flag(C, temp & 0xFF00);
    set_flag(Z, ((temp) & 0x00FF) == 0);
    set_flag(V, (temp ^ (uint16_t)regs.a) & (temp ^ value) & 0x0080);
    set_flag(N, temp & 0x0080);
    regs.a = temp & 0x00FF;
    return 1;
}

static uint8_t SEC()
{
    set_flag(C, 1);
    return 0;
}

static uint8_t SED()
{
    set_flag(D, 1);
    return 0;
}

static uint8_t SEI()
{
    set_flag(I, 1);
    return 0;
}

static uint8_t STA()
{
    write(addr_abs, regs.a);
    return 0;
}

static uint8_t STX()
{
    write(addr_abs, regs.x);
    return 0;
}

static uint8_t STY()
{
    write(addr_abs, regs.y);
    return 0;
}

static uint8_t TAX()
{
    regs.x = regs.a;
    set_flag(Z, regs.x == 0x00);
    set_flag(N, regs.x & 0x80);
    return 0;
}

static uint8_t TAY()
{
    regs.y = regs.a;
    set_flag(Z, regs.y == 0x00);
    set_flag(N, regs.y == 0x80);
    return 0;
}

static uint8_t TSX()
{
    regs.x = regs.sp;
    set_flag(Z, regs.x == 0x00);
    set_flag(N, regs.x & 0x80);

    return 0;
}

static uint8_t TXA()
{
    regs.a = regs.x;
    set_flag(Z, regs.a == 0x00);
    set_flag(N, regs.a & 0x80);

    return 0;
}

static uint8_t TXS()
{
    regs.sp = regs.x;
    return 0;
}

static uint8_t TYA()
{
    regs.y = regs.a;
    set_flag(Z, regs.a == 0x00);
    set_flag(N, regs.a & 0x80);

    return 0;
}

/******************************************************************************
 * Setup area for the op codes
 *
 *
 */
static void set_operation(uint8_t op_code, char *name, uint8_t (*operation)(), uint8_t (*add_func)(), uint8_t cycles)
{
    strncpy(op_codes[op_code].op_name, name, sizeof(op_codes[op_code].op_name));
    op_codes[op_code].operation = operation;
    op_codes[op_code].addr_mode = add_func;
    op_codes[op_code].cycles = cycles;
}

static void set_all_unk()
{
    for (int i = 0; i <= 0xFF; i++)
    {
        strncpy(op_codes[i].op_name, "???", sizeof(op_codes[i].op_name));
        op_codes[i].operation = &UNK;
        op_codes[i].addr_mode = &UNK;
        op_codes[i].cycles = 1;
    }
}

static void set_adc()
{
    set_operation(0x69, "ADC", &ADC, &IMM, 2);
    set_operation(0x65, "ADC", &ADC, &ZP0, 3);
    set_operation(0x75, "ADC", &ADC, &ZPX, 4);
    set_operation(0x6D, "ADC", &ADC, &ABS, 4);
    set_operation(0x7D, "ADC", &ADC, &ABX, 4);
    set_operation(0x79, "ADC", &ADC, &ABY, 4);
    set_operation(0x61, "ADC", &ADC, &IDX, 6);
    set_operation(0x71, "ADC", &ADC, &IDY, 5);
}

static void set_and()
{
    set_operation(0x29, "AND", &AND, &IMM, 2);
    set_operation(0x25, "AND", &AND, &ZP0, 3);
    set_operation(0x35, "AND", &AND, &ZPX, 4);
    set_operation(0x2D, "AND", &AND, &ABS, 4);
    set_operation(0x3D, "AND", &AND, &ABX, 4);
    set_operation(0x39, "AND", &AND, &ABY, 4);
    set_operation(0x21, "AND", &AND, &IDX, 6);
    set_operation(0x31, "AND", &AND, &IDY, 5);
}

static void set_asl()
{
    set_operation(0x0A, "ASL", &ASL, &ACC, 2);
    set_operation(0x06, "ASL", &ASL, &ZP0, 5);
    set_operation(0x16, "ASL", &ASL, &ZPX, 6);
    set_operation(0x0E, "ASL", &ASL, &ABS, 6);
    set_operation(0x1E, "ASL", &ASL, &ABX, 7);
}

static void set_branch_ops()
{
    set_operation(0x90, "BCC", &BCC, &REL, 2);
    set_operation(0xB0, "BCS", &BCS, &REL, 2);
    set_operation(0xF0, "BEQ", &BEQ, &REL, 2);
    set_operation(0x30, "BMI", &BMI, &REL, 2);
    set_operation(0xD0, "BNE", &BNE, &REL, 2);
    set_operation(0x10, "BPL", &BPL, &REL, 2);
    set_operation(0x50, "BVC", &BVC, &REL, 2);
    set_operation(0x70, "BVS", &BVS, &REL, 2);
}

static void set_compare_ops()
{
    set_operation(0x24, "BIT", &BIT, &ZP0, 3);
    set_operation(0x2c, "BIT", &BIT, &ABS, 4);

    // CMP operations; all flavors
    set_operation(0xC9, "CMP", &CMP, &IMM, 2);
    set_operation(0xC5, "CMP", &CMP, &ZP0, 3);
    set_operation(0xD5, "CMP", &CMP, &ZPX, 4);
    set_operation(0xCD, "CMP", &CMP, &ABS, 4);
    set_operation(0xDD, "CMP", &CMP, &ABX, 4);
    set_operation(0xD9, "CMP", &CMP, &ABY, 4);
    set_operation(0xC1, "CMP", &CMP, &IDX, 6);
    set_operation(0xD1, "CMP", &CMP, &IDY, 5);

    // CPX operations
    set_operation(0xE0, "CPX", &CPX, &IMM, 2);
    set_operation(0xE4, "CPX", &CPX, &ZP0, 3);
    set_operation(0xEC, "CPX", &CPX, &ABS, 4);

    // CPY operations
    set_operation(0xc0, "CPY", &CPY, &IMM, 2);
    set_operation(0xc4, "CPY", &CPY, &ZP0, 3);
    set_operation(0xcc, "CPY", &CPY, &ABS, 4);

    // ORA operations
    set_operation(0x09, "ORA", &ORA, &IMM, 2);
    set_operation(0x05, "ORA", &ORA, &ZP0, 3);
    set_operation(0x15, "ORA", &ORA, &ZPX, 4);
    set_operation(0x0d, "ORA", &ORA, &ABS, 4);
    set_operation(0x1d, "ORA", &ORA, &ABX, 4);
    set_operation(0x19, "ORA", &ORA, &ABY, 4);
    set_operation(0x01, "ORA", &ORA, &IDX, 6);
    set_operation(0x11, "ORA", &ORA, &IDY, 5);
}

static void set_break()
{
    set_operation(0x00, "BRK", &BRK, &IMP, 7);
}

static void set_clear_ops()
{
    set_operation(0x18, "CLC", &CLC, &IMP, 2);
    set_operation(0xD8, "CLD", &CLD, &IMP, 2);
    set_operation(0x58, "CLI", &CLI, &IMP, 2);
    set_operation(0xB8, "CLV", &CLV, &IMP, 2);
}

static void set_decrement_ops()
{
    // DEC memory by one
    set_operation(0xC6, "DEC", &DEC, &ZP0, 5);
    set_operation(0xD6, "DEC", &DEC, &ZPX, 6);
    set_operation(0xCE, "DEC", &DEC, &ABS, 6);
    set_operation(0xDE, "DEC", &DEC, &ABX, 7);

    // DEX index X by one
    set_operation(0xCA, "DEX", &DEX, &IMP, 2);

    // DEY index Y by one
    set_operation(0x88, "DEY", &DEY, &IMP, 2);
}

static void set_exclusive_or()
{
    set_operation(0x49, "EOR", &EOR, &IMM, 2);
    set_operation(0x45, "EOR", &EOR, &ZP0, 3);
    set_operation(0x55, "EOR", &EOR, &ZPX, 4);
    set_operation(0x4d, "EOR", &EOR, &ABS, 4);
    set_operation(0x5d, "EOR", &EOR, &ABX, 4);
    set_operation(0x59, "EOR", &EOR, &ABY, 4);
    set_operation(0x41, "EOR", &EOR, &IDX, 6);
    set_operation(0x51, "EOR", &EOR, &IDY, 5);
}

static void set_increment_ops()
{
    // INC memory by one
    set_operation(0xe6, "INC", &INC, &ZP0, 5);
    set_operation(0xF6, "INC", &INC, &ZPX, 6);
    set_operation(0xEE, "INC", &INC, &ABS, 6);
    set_operation(0xFE, "INC", &INC, &ABX, 7);

    // DEX index X by one
    set_operation(0xE8, "INX", &INX, &IMP, 2);

    // DEY index Y by one
    set_operation(0xC8, "INY", &INY, &IMP, 2);
}

static void set_jump_ops()
{
    // JMP
    set_operation(0x4c, "JMP", &JMP, &ABS, 3);
    set_operation(0x6c, "JMP", &JMP, &IND, 5);

    // JSR
    set_operation(0x20, "JSR", &JSR, &ABS, 6);
}

static void set_load_ops()
{
    // LDA Load Accumulator
    set_operation(0xA9, "LDA", &LDA, &IMM, 2);
    set_operation(0xA5, "LDA", &LDA, &ZP0, 3);
    set_operation(0xb5, "LDA", &LDA, &ZPX, 4);
    set_operation(0xad, "LDA", &LDA, &ABS, 4);
    set_operation(0xbd, "LDA", &LDA, &ABX, 4);
    set_operation(0xb9, "LDA", &LDA, &ABY, 4);
    set_operation(0xa1, "LDA", &LDA, &IDX, 6);
    set_operation(0xb1, "LDA", &LDA, &IDY, 5);

    // LDX load X with mem
    set_operation(0xa2, "LDX", &LDX, &IMM, 2);
    set_operation(0xa6, "LDX", &LDX, &ZP0, 3);
    set_operation(0xb6, "LDX", &LDX, &ZPY, 4);
    set_operation(0xae, "LDX", &LDX, &ABS, 4);
    set_operation(0xbe, "LDX", &LDX, &ABY, 4);

    // LDY load Y with mem
    set_operation(0xa0, "LDY", &LDY, &IMM, 2);
    set_operation(0xa4, "LDY", &LDY, &ZP0, 3);
    set_operation(0xb4, "LDY", &LDY, &ZPX, 4);
    set_operation(0xac, "LDY", &LDY, &ABS, 4);
    set_operation(0xbc, "LDY", &LDY, &ABX, 4);
}

static void set_shift_ops()
{
    // LSR
    set_operation(0x4a, "LSR", &LSR, &ACC, 2);
    set_operation(0x46, "LSR", &LSR, &ZP0, 5);
    set_operation(0x56, "LSR", &LSR, &ZPX, 6);
    set_operation(0x4e, "LSR", &LSR, &ABS, 6);
    set_operation(0x5e, "LSR", &LSR, &ABX, 7);
}

static void set_nop()
{
    set_operation(0xea, "NOP", &NOP, &IMP, 2);
}

static void set_stack_ops()
{
    set_operation(0x48, "PHA", &PHA, &IMP, 3);
    set_operation(0x08, "PHP", &PHP, &IMP, 3);
    set_operation(0x68, "PLA", &PLA, &IMP, 4);
    set_operation(0x28, "PLP", &PLP, &IMP, 4);
}

static void set_bit_ops()
{
    // ROL rotate left
    set_operation(0x2a, "ROL", &ROL, &ACC, 2);
    set_operation(0x26, "ROL", &ROL, &ZP0, 5);
    set_operation(0x36, "ROL", &ROL, &ZPX, 6);
    set_operation(0x2e, "ROL", &ROL, &ABS, 6);
    set_operation(0x3e, "ROL", &ROL, &ABX, 7);

    // ROR rotate right
    set_operation(0x6a, "ROR", &ROR, &ACC, 2);
    set_operation(0x66, "ROR", &ROR, &ZP0, 5);
    set_operation(0x76, "ROR", &ROR, &ZPX, 6);
    set_operation(0x6e, "ROR", &ROR, &ABS, 6);
    set_operation(0x7e, "ROR", &ROR, &ABX, 7);
}

static void set_return_ops()
{
    set_operation(0x40, "RTI", &RTI, &IMP, 6);
    set_operation(0x60, "RTS", &RTS, &IMP, 6);
}

static void set_subtract()
{
    set_operation(0xe9, "SBC", &SBC, &IMM, 2);
    set_operation(0xe5, "SBC", &SBC, &ZP0, 3);
    set_operation(0xf5, "SBC", &SBC, &ZPX, 4);
    set_operation(0xed, "SBC", &SBC, &ABS, 4);
    set_operation(0xfd, "SBC", &SBC, &ABX, 4);
    set_operation(0xf9, "SBC", &SBC, &ABY, 4);
    set_operation(0xe1, "SBC", &SBC, &IDX, 6);
    set_operation(0xf1, "SBC", &SBC, &IDY, 5);
}

static void set_flag_ops()
{
    set_operation(0x38, "SEC", &SEC, &IMP, 2);
    set_operation(0xF8, "SED", &SED, &IMP, 2);
    set_operation(0x78, "SEI", &SEI, &IMP, 2);
}

static void set_store_reg_ops()
{
    // STA accumulator to mem
    set_operation(0x85, "STA", &STA, &ZP0, 3);
    set_operation(0x95, "STA", &STA, &ZPX, 4);
    set_operation(0x8D, "STA", &STA, &ABS, 4);
    set_operation(0x9D, "STA", &STA, &ABX, 5);
    set_operation(0x99, "STA", &STA, &ABY, 5);
    set_operation(0x81, "STA", &STA, &IDX, 6);
    set_operation(0x91, "STA", &STA, &IDY, 6);

    // STX X to mem
    set_operation(0x86, "STX", &STX, &ZP0, 3);
    set_operation(0x96, "STX", &STX, &ZPY, 4);
    set_operation(0x8e, "STX", &STX, &ABS, 4);

    // STY Y to mem
    set_operation(0x84, "STY", &STY, &ZP0, 3);
    set_operation(0x94, "STY", &STY, &ZPX, 4);
    set_operation(0x8c, "STY", &STY, &ABS, 4);
}

static void set_transfer_ops()
{
    set_operation(0xaa, "TAX", &TAX, &IMP, 2);
    set_operation(0xa8, "TAY", &TAY, &IMP, 2);
    set_operation(0xba, "TSX", &TSX, &IMP, 2);
    set_operation(0x8a, "TXA", &TXA, &IMP, 2);
    set_operation(0x9a, "TXS", &TXS, &IMP, 2);
    set_operation(0x98, "TYA", &TYA, &IMP, 2);
}

static void setup_instruction_set()
{
    set_all_unk();
    set_adc();
    set_and();
    set_asl();
    set_branch_ops();
    set_compare_ops();
    set_break();
    set_clear_ops();
    set_decrement_ops();
    set_exclusive_or();
    set_increment_ops();
    set_jump_ops();
    set_load_ops();
    set_shift_ops();
    set_nop();
    set_stack_ops();
    set_bit_ops();
    set_return_ops();
    set_subtract();
    set_flag_ops();
    set_store_reg_ops();
    set_transfer_ops();
}

static void write(uint16_t addr, uint8_t value)
{
    if (pccb_bus)
    {
        pccb_bus->write(addr, value);
    }
}

static uint8_t read(uint16_t addr)
{
    if (pccb_bus)
    {
        return pccb_bus->read(addr);
    }

    return 0;
}

static uint8_t get_flag(FLAGS6502 f)
{
    return ((regs.flags & f) > 0) ? 1 : 0;
}

static void set_flag(FLAGS6502 f, bool v)
{
    if (v) // set flag on
    {
        regs.flags |= f;
    }
    else
    {
        regs.flags &= (~f);
    }
}

/************************************************************************************
 * functions callable from application running the processor
 */
void initialize_c6502(ccb_bus* pBus)
{
    pccb_bus = pBus;
    setup_instruction_set();
    reset();
}

void clock()
{
    if (cycles == 0)
    {
        // get next instruction
        opcode = read(regs.pc);
        regs.pc++;

        cycles = op_codes[opcode].cycles;

        uint8_t additional_cycle1 = op_codes[opcode].addr_mode();

        uint8_t additional_cycle2 = op_codes[opcode].operation();

        cycles += (additional_cycle1 & additional_cycle2);
    }
    cycles--;
}

bool complete()
{
    return cycles == 0;
}

const cpu_state* get_cpu_state()
{
    return &regs;
}

void reset()
{
    regs.a = 0;
    regs.x = 0;
    regs.y = 0;
    regs.sp = 0xFD;
    regs.flags = 0x00 | U;

    addr_abs = 0xFFFC;
    uint16_t lo = read(addr_abs + 0);
    uint16_t hi = read(addr_abs + 1);
    regs.pc = (hi << 8) | lo;

    addr_rel = 0x0000;
    addr_abs = 0x0000;
    fetched = 0x00;

    cycles = 8;
}

void irq()
{
    if (get_flag(I) == 0)
    {
        write(0x0100 + regs.sp, (regs.pc >> 8) & 0x00FF);
        regs.sp--;
        write(0x0100 + regs.sp, regs.pc & 0x00FF);
        regs.sp--;

        set_flag(B, 0);
        set_flag(U, 1);
        set_flag(I, 1);
        write(0x0100 + regs.sp, regs.flags);
        regs.sp--;

        addr_abs = 0xFFFE;
        uint16_t lo = read(addr_abs + 0);
        uint16_t hi = read(addr_abs + 1);
        regs.pc = (hi << 8) | lo;

        cycles = 7;
    }
}

void nmi()
{
    write(0x0100 + regs.sp, (regs.pc >> 8) & 0x00FF);
    regs.sp--;
    write(0x0100 + regs.sp, regs.pc & 0x00FF);
    regs.sp--;

    set_flag(B, 0);
    set_flag(U, 1);
    set_flag(I, 1);
    write(0x0100 + regs.sp, regs.flags);
    regs.sp--;

    addr_abs = 0xFFFA;
    uint16_t lo = read(addr_abs + 0);
    uint16_t hi = read(addr_abs + 1);
    regs.pc = (hi << 8) | lo;

    cycles = 7;
}