CC 		= gcc
CFLAGS 		= -Wall -pthread
LDFLAGS 	= 
OBJFILES 	= functions.o main.o
TARGET 		= project

all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
