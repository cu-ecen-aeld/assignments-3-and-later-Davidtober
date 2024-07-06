SRC := writer.c
TARGET = writer
OBJS := $(SRC:.c=.o)

ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
CC=aarch64-none-linux-gnu-gcc
endif

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map