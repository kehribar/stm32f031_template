#------------------------------------------------------------------------------
# Makefile for STM32F031
# 03-30-2015 E. Brombaugh -- initial work
# 25-07-2015 ihsan Kehribar -- later modified
#------------------------------------------------------------------------------

# Sub directories
VPATH = BackendSupport/CMSIS BackendSupport/StdPeriph HardwareLayer

# Object files
OBJECTS  = main.o systick_delay.o
OBJECTS += startup_stm32f030.o system_stm32f0xx.o stm32f0xx_rcc.o

# Use 'launchpad.net/gcc-arm-embedded' version for a complete toolchain setup
TOOLCHAIN_FOLDER = /Users/kehribar/gcc-arm-none-eabi-4_9-2015q2/bin/

#------------------------------------------------------------------------------
# Almost nothing to play with down there ... Don't change.
#------------------------------------------------------------------------------

# Must define the MCU type
CDEFS = -DSTM32F031 -DUSE_STDPERIPH_DRIVER -DARM_MATH_CM0

# Linker script
LDSCRIPT = BackendSupport/stm32f031_linker.ld

# Optimization level, can be [0, 1, 2, 3, s].
OPTLVL:= s 

# Object location settings
OBJDIR  = obj
OBJECTS_O := $(addprefix $(OBJDIR)/,$(OBJECTS))

# Compiler and linker flags
COMMONFLAGS = -O$(OPTLVL) -g -ffunction-sections -std=c99 -Wall
MCUFLAGS    = -mthumb -mcpu=cortex-m0

CFLAGS  = $(COMMONFLAGS) $(MCUFLAGS) -I. -IBackendSupport/CMSIS -IBackendSupport/StdPeriph -IHardwareLayer $(CDEFS)
CFLAGS += -ffreestanding -nostdlib

LDFLAGS  = $(COMMONFLAGS) $(MCUFLAGS) -fno-exceptions
LDFLAGS += -fdata-sections -nostartfiles -Wl,--gc-sections,-T$(LDSCRIPT),-Map=$(OBJDIR)/main.map

# Executables
ARCH   = $(TOOLCHAIN_FOLDER)arm-none-eabi
CC     = $(ARCH)-gcc
LD     = $(ARCH)-ld -v
AS     = $(ARCH)-as
OBJCPY = $(ARCH)-objcopy
OBJDMP = $(ARCH)-objdump
GDB    = $(ARCH)-gdb
SIZE   = $(ARCH)-size
OPENOCD = openocd

CPFLAGS = --output-target=binary
ODFLAGS	= -x --syms

# Targets
all: main.bin

clean:
	-rm -rf $(OBJDIR)

main.ihex: main.elf
	$(OBJCPY) --output-target=ihex main.elf main.ihex

main.bin: main.elf 
	$(OBJCPY) $(CPFLAGS) $(OBJDIR)/main.elf $(OBJDIR)/main.bin
	$(OBJDMP) -d $(OBJDIR)/main.elf > $(OBJDIR)/main.dis
	$(SIZE) $(OBJDIR)/main.elf

main.elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) -o $(OBJDIR)/main.elf $(LDFLAGS) $(OBJECTS_O) -lnosys

main.o: 
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/main.o main.c

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $(OBJDIR)/$@ $<

%.o: %.s
	$(AS) -c -o $(OBJDIR)/$@ $<

stlink-flash:
	st-flash write $(OBJDIR)/main.bin 0x8000000

openocd-flash:
	$(OPENOCD) -f BackendSupport/openocd.cfg -c flash_chip

debugserver:
	$(OPENOCD) -f BackendSupport/openocd.cfg

## Run 'make debugserver' in a seperate console window before running 'make debug'
debug:
	make all && $(GDB) obj/main.elf \
		-ex "target remote localhost:3333" \
		-ex "load" \
		-ex "b main" \
		-ex "monitor reset halt" \
		-ex "continue" \
		-ex "-"

iterate:
	make clean && make all && make openocd-flash

-include $(shell mkdir $(OBJDIR) 2>/dev/null) $(wildcard $(OBJDIR)/*.d)

#------------------------------------------------------------------------------