#include <stdio.h>
#include "ccbRamBus.h"

int main(void)
{
	ccb_bus bus;
	constructor(&bus);
	if (bus.initialize() == 0)
	{
		bus.write(0x8000, 0x20);
		if (bus.read(0x8000) == 0x20)
		{
			printf("Bus worked successfully\n");
		}
		else
		{
			printf("Something is wrong with the bus");
		}
		bus.shutdown();
	}


	return 0;
}