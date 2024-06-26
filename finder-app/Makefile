SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)

ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
CC=/home/david/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc
endif

$(info CC is $(CC))
$(info CROSS_COMPILE is $(CROSS_COMPILE))

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map