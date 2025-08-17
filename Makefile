CC=gcc
CFLAGS=-g -Wall
INCLUDES=-Iinclude
SRC_C6502=src/c6502/c6502.c src/c6502/ccbRamBus.c
TESTS=src/tests/emutest.c src/tests/testbus.c
OBJS_C6502=$(SRC_C6502:.c=.o)
OBJS_EMUTEST=src/tests/emutest.o $(OBJS_C6502)
OBJS_TESTBUS=src/tests/testbus.o $(OBJS_C6502)

all: emutest testbus

emutest: $(OBJS_EMUTEST)
	$(CC) $(CFLAGS) $(INCLUDES) -o emutest $(OBJS_EMUTEST)

testbus: $(OBJS_TESTBUS)
	$(CC) $(CFLAGS) $(INCLUDES) -o testbus $(OBJS_TESTBUS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f src/c6502/*.o src/tests/*.o emutest testbus
