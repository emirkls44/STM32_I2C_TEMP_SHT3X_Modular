# Host-side unit tests for the SHT3x driver: `make -C tests`
CC     ?= cc
CFLAGS += -std=c11 -Wall -Wextra -Werror -pedantic -g -fsanitize=address,undefined

test: test_sht3x
	./test_sht3x

# fakes/ provides minimal CMSIS/HAL headers so the driver builds on a PC
test_sht3x: test_sht3x.c ../Core/Src/sht3x.c ../Core/Inc/sht3x.h ../Core/Inc/i2c.h
	$(CC) $(CFLAGS) -Ifakes -I../Core/Inc test_sht3x.c ../Core/Src/sht3x.c -lm -o $@

clean:
	rm -f test_sht3x

.PHONY: test clean
