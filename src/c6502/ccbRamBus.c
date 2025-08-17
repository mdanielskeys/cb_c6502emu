
#include <stdlib.h>
#include <stdint.h>
#include "ccbRamBus.h"
#include "ccbBus.h"

static uint8_t* ram = NULL;


static uint16_t initialize()
{
    ram = (uint8_t*)calloc(0x10000, sizeof(uint8_t));
    if (ram == NULL)
    {
        // an error occurred return bad init
        return RAMBUS_INIT_ERR;
    }
    return RAMBUS_INIT_OK;
}


static void shutdown()
{
    if (ram != NULL) {
        free(ram);
        ram = NULL;
    }
}


static void write(uint16_t addr, uint8_t value)
{
    if (ram != NULL)
        ram[addr & 0xFFFF] = value;
}


static uint8_t read(uint16_t addr)
{
    if (ram != NULL)
        return ram[addr & 0xFFFF];
    return 0;
}


void bus_constructor(ccb_bus* bus_def)
{
    bus_def->initialize = &initialize;
    bus_def->shutdown = &shutdown;
    bus_def->write = &write;
    bus_def->read = &read;
}
