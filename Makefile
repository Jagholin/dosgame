OUTPUT = GAME.exe
#VPATH=src
CFILES = $(wildcard *.C)
OBJS = $(CFILES:.C=.O) # IRQWRAP.O
LDFLAGS += -lm
CFLAGS += -std=gnu99
CC ?= gcc

all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	$(CC) -ggdb -o $(OUTPUT) $(OBJS) $(LDFLAGS) 

%.O: %.C *.H
	$(CC) -c -x c $(CFLAGS) -ggdb $< -o $@

#IRQWRAP.O: IRQWRAP.S
#	$(CC) -c IRQWRAP.S -o $@

.PHONY: clean

clean:
	-rm -f *.O $(OUTPUT)

