#ifndef CCB_BUS_DEF_H
#define CCB_BUS_DEF_H
#include <stdint.h>

#define RAMBUS_INIT_OK 0
#define RAMBUS_INIT_ERR UINT16_MAX

typedef struct CCB_BUS
{
	uint16_t(*initialize)();
	void(*shutdown)();
	void(*write)(uint16_t, uint8_t);
	uint8_t(*read)(uint16_t);
}ccb_bus;

#endif