#include <stdio.h>
#include <string.h>
#include <time.h>
#include "c6502.h"
#include "ccbRamBus.h"

static uint8_t get_flag(FLAGS6502 flag)
{
    const cpu_state *pcpu_state = get_cpu_state();
    return (pcpu_state->flags & flag) ? 1 : 0;
}

static void set_reset_vector(ccb_bus *pccb_bus, uint16_t prog_start)
{
    uint16_t reset_vector = 0xfffc;

    pccb_bus->write(reset_vector, prog_start & 0x00FF);            // lo byte
    pccb_bus->write(reset_vector + 1, (prog_start >> 8) & 0x00FF); // high byte
}

static void load_program(ccb_bus *pccb_bus, uint8_t *prog, uint16_t start_address)
{
    int offset = 0;
    while (prog[offset] != 0)
    {
        pccb_bus->write(start_address + offset, prog[offset]);
        offset++;
    }
    set_reset_vector(pccb_bus, 0x8000);
}

static void run_program(uint16_t start_address, uint16_t prog_size)
{
    struct timespec tsstart, tnow;
    const cpu_state *pcpu_state = get_cpu_state();
    reset();
    timespec_get(&tsstart, TIME_UTC);
    do
    {
        timespec_get(&tnow, TIME_UTC);
        if (((uint64_t)tnow.tv_nsec - (uint64_t)tsstart.tv_nsec) >= 500)
        {
            cpu_clock();
            tsstart = tnow;
        }
    } while ((pcpu_state->pc < (start_address + prog_size)) || (!complete()));
}

static void load_and_run_test_prog(ccb_bus *pccb_bus, uint8_t *prog, char *test_name)
{
    uint16_t start_address = 0x8000;
    printf("\n%s loading program\n", test_name);
    load_program(pccb_bus, prog, start_address);
    printf("%s running program\n", test_name);
    run_program(start_address, strnlen(prog, 13));
}

static void test_reg_a(uint8_t expected_value, char *test_name)
{
    const cpu_state *pcpu_state = get_cpu_state();

    printf("%s program reg_a result value expected 0x%02x ", test_name, expected_value);
    if (pcpu_state->a == expected_value)
    {
        printf("ran correctly - OK\n");
    }
    else
    {
        printf("got 0x%02x - FAIL\n", pcpu_state->a);
    }
}

static void test_reg_x(uint8_t expected_value, char *test_name)
{
    const cpu_state *pcpu_state = get_cpu_state();

    printf("%s program reg_x result value expected 0x%02x ", test_name, expected_value);
    if (pcpu_state->x == expected_value)
    {
        printf("ran correctly - OK\n");
    }
    else
    {
        printf("got 0x%02x - FAIL\n", pcpu_state->x);
    }
}

static void test_reg_y(uint8_t expected_value, char *test_name)
{
    const cpu_state *pcpu_state = get_cpu_state();

    printf("%s program reg_y result value expected 0x%02x ", test_name, expected_value);
    if (pcpu_state->y == expected_value)
    {
        printf("ran correctly - OK\n");
    }
    else
    {
        printf("got 0x%02x - FAIL", pcpu_state->y);
    }
}

static void test_flags(uint8_t expected_value, FLAGS6502 flag, char *flag_name, char *test_name)
{
    const cpu_state *pcpu_state = get_cpu_state();

    printf("%s program flag '%s' result value expected %d ", test_name, flag_name, expected_value);
    if (get_flag(flag) == expected_value)
    {
        printf("ran correctly - OK\n");
    }
    else
    {
        printf("got %d - FAIL\n", expected_value, get_flag(flag));
    }
}

static void test_indirect_zero_page(ccb_bus *pccb_bus)
{
    /*
    LDA #$20
    STA $11
    LDA #$10
    STA $10
    LDX #$01
    LDA $10,X
    */
    char test_name[] = "test_indirect_zero_page";
    uint8_t prog[] = {0xA9, 0x20, 0x85, 0x11, 0xA9, 0x10, 0x85, 0x10, 0xA2, 0x01, 0xB5, 0x10, 0x00};
    load_and_run_test_prog(pccb_bus, prog, test_name);
    test_reg_a(0x20, test_name);
    test_reg_x(0x01, test_name);
    test_flags(0, N, "N", test_name);
    test_flags(0, Z, "Z", test_name);
}

static void test_lda_immediate(ccb_bus *pccb_bus)
{
    /*
    LDA #$20
    */
    char test_name[] = "test_lda_immediate";
    uint8_t prog[] = {0xA9, 0x20, 0x00};
    load_and_run_test_prog(pccb_bus, prog, test_name);
    test_reg_a(0x20, test_name);
    test_flags(0, N, "N", test_name);
    test_flags(0, Z, "Z", test_name);
}

static void test_lda_immediate_neg(ccb_bus *pccb_bus)
{
    /*
    LDA #$FF
    */
    char test_name[] = "test_lda_immediate_neg";
    uint8_t prog[] = {0xA9, 0xFF, 0x00};
    load_and_run_test_prog(pccb_bus, prog, test_name);
    test_reg_a(0xFF, test_name);
    test_flags(1, N, "N", test_name);
    test_flags(0, Z, "Z", test_name);
}

static void test_stack_push_pull(ccb_bus *pccb_bus)
{
    /*
    LDA #$20
    PHA
    LDA #$15
    TAX
    PLA
    */
    char test_name[] = "test_stack_push_pull";
    uint8_t prog[] = {0xA9, 0x20, 0x48, 0xA9, 0x15, 0xAA, 0x68, 0x00};
    load_and_run_test_prog(pccb_bus, prog, test_name);
    test_reg_a(0x20, test_name);
    test_reg_x(0x15, test_name);
    test_flags(0, N, "N", test_name);
    test_flags(0, Z, "Z", test_name);
}

int main(void)
{
    ccb_bus ram_bus;
    bus_constructor(&ram_bus);
    if (ram_bus.initialize() == 0)
    {
        // successful initialization of bus
        initialize_c6502(&ram_bus);
        test_indirect_zero_page(&ram_bus);
        test_lda_immediate(&ram_bus);
        test_lda_immediate_neg(&ram_bus);
        test_stack_push_pull(&ram_bus);

        ram_bus.shutdown();
    }
    else
    {
        printf("Error starting emulator\n");
        return -1;
    }

    return 0;
}