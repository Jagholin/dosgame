OUTPUT=GAME.exe
#VPATH=src
CFILES=$(wildcard *.C)
OBJS=$(CFILES:.C=.O)

%.O: %.C *.H
	$(CC) -c $(CFLAGS) $< -o $@

$(OUTPUT): $(OBJS)
	$(CC) $(LDFLAGS) -o $(OUTPUT) $(OBJS)

all: $(OUTPUT)

.PHONY: clean

clean:
	rm -f *.O $(OUTPUT)

