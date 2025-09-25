OUTPUT = game.exe
# It's a bit of a pain to transform the case, so we just use 2nd var
OUTPUT_L = GAME.EXE
#VPATH=src
CFILES = $(wildcard *.c)
OBJS = $(CFILES:.c=.o) # IRQWRAP.O
LDFLAGS += -lm
CFLAGS += -std=gnu99
CC ?= gcc

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) -ggdb -o $(OUTPUT) $(OBJS) $(LDFLAGS) 

%.o: %.c *.h
	$(CC) -c -x c $(CFLAGS) -ggdb $< -o $@

#IRQWRAP.O: IRQWRAP.S
#	$(CC) -c IRQWRAP.S -o $@

.PHONY: clean

clean:
	-rm -f *.o *.O $(OUTPUT) $(OUTPUT_L)

