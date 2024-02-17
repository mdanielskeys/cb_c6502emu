#include "ccbRamBus.h"
#include "ccbBus.h"

uint8_t* ram;

static uint16_t initialize()
{
    ram = (uint8_t*)calloc(0x10000, sizeof(uint8_t));
    if (ram == NULL)
    {
        // an error occurred return bad init
        return -1;
    }

    return 0;
}

static void shutdown()
{
    if (ram != NULL)
        free(ram);
}

static void write(uint16_t addr, uint8_t value)
{
    if (ram != NULL && (addr >= 0x0000 && addr <= 0xFFFF))
        ram[addr] = value;
}

static uint8_t read(uint16_t addr)
{
    if (ram != NULL && (addr >= 0x0000 && addr <= 0xFFFF))
        return ram[addr];

    return 0;
}

void bus_constructor(ccb_bus* bus_def)
{
    bus_def->initialize = &initialize;
    bus_def->shutdown = &shutdown;
    bus_def->write = &write;
    bus_def->read = &read;
}
