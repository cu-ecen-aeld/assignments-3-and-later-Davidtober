SRC := aesdsocket.c
TARGET = aesdsocket
OBJS := $(SRC:.c=.o)

CC = $(CROSS_COMPILE)gcc
CFLAGS := $(CFLAGS) -Wall -Werror -g -D_GNU_SOURCE

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map