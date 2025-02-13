CC = gcc
CFLAGS = -Wall -Wextra -g
SRCDIR = src
OBJDIR = obj
BINDIR = bin
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET = jshell
TARGET_PATH = $(BINDIR)/$(TARGET)

all: $(TARGET_PATH)

$(TARGET_PATH): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(TARGET_PATH) $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)
