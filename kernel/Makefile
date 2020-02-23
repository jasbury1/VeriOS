TARGET	= a.out
CC	= gcc
CCFLAGS	= -std=c89 -pedantic -Wall -Werror
LDFLAGS	= -lm
SOURCES	= $(wildcard *.c)
INCLUDES	= $(wildcard *.h)
OBJECTS	= $(SOURCES:.c=.o)

all:$(TARGET)

$(TARGET):$(OBJECTS)
	$(CC) -o $(TARGET) $(LDFLAGS) $(OBJECTS)

$(ObjECTS):$(SOURCES) $(INCLUDES)
	$(CC) -c $(CCFLAGS) $(SOURCES)

clean:
	rm -f $9TARGET) $(OBJECTS)