OUTPUT = GAME.exe
#VPATH=src
CFILES = $(wildcard *.C)
OBJS = $(CFILES:.C=.O) IRQWRAP.O
LDFLAGS += -lm
CFLAGS += -std=gnu99
CC ?= gcc

%.O: %.C *.H
	$(CC) -c -x c $(CFLAGS) -ggdb $< -o $@

IRQWRAP.O: IRQWRAP.S
	$(CC) -c IRQWRAP.S -o $@

$(OUTPUT): $(OBJS)
	$(CC) -ggdb -o $(OUTPUT) $(OBJS) $(LDFLAGS) 

all: $(OUTPUT)

.PHONY: clean

clean:
	rm -f *.O $(OUTPUT)

