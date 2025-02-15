CC = gcc
CFLAGS = -Wall -Wextra -g
SRCDIR = src
OBJDIR = obj
BINDIR = bin
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))
TARGET = jshell
TARGET_PATH = $(BINDIR)/$(TARGET)
RCFILE = .jshellrc

all: $(TARGET_PATH) install

$(TARGET_PATH): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $(TARGET_PATH) $(OBJECTS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

install: $(TARGET_PATH)
	@echo "Installing jshell..."
	@mkdir -p $(HOME)/bin
	@cp $(TARGET_PATH) $(HOME)/bin/jshell-bin
	@cp scripts/jshell-wrapper $(HOME)/bin/jshell
	@chmod +x $(HOME)/bin/jshell-bin $(HOME)/bin/jshell
	@cp -f $(RCFILE) $(HOME)/.jshellrc
	@chmod 644 $(HOME)/.jshellrc
	@if ! grep -q "PATH=\"$(HOME)/bin:\$$PATH\"" $(HOME)/.bashrc; then \
		echo 'export PATH="$$HOME/bin:$$PATH"' >> $(HOME)/.bashrc; \
	fi
	@echo "Installation complete. Please run:"
	@echo "    source ~/.bashrc && source ~/.jshellrc"

update-rc:
	@cp -f $(RCFILE) $(HOME)/.jshellrc
	@chmod 644 $(HOME)/.jshellrc
	@echo "Updated ~/.jshellrc. Please run:"
	@echo "    source ~/.jshellrc"

# Add development target that doesn't install to system
dev: $(TARGET_PATH)
	@echo "Built jshell in ./bin"
	@echo "Run with: ./bin/jshell"

uninstall:
	@echo "Uninstalling jshell..."
	@rm -f $(HOME)/bin/jshell
	@rm -f $(HOME)/bin/jshell-bin
	@rm -f $(HOME)/.jshellrc
	@echo "Uninstallation complete."

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f $(HOME)/bin/$(TARGET)

.PHONY: all install uninstall clean
