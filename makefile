CC = gcc
SRCDIR = src
BINDIR = bin

# Targets
TARGETS = $(BINDIR)/director $(BINDIR)/worker $(BINDIR)/user $(BINDIR)/ticket_erogator
SHARED_OBJ = $(BINDIR)/shared_function.o

# Default target
all: $(BINDIR) $(SHARED_OBJ) $(TARGETS)

# Create bin directory if it doesn't exist
$(BINDIR):
	mkdir -p $(BINDIR)

# Compile shared object file
$(SHARED_OBJ): $(SRCDIR)/shared_function.c
	$(CC) -c $< -o $@

# Compile binaries
$(BINDIR)/director: $(SRCDIR)/director.c $(SHARED_OBJ)
	$(CC) $^ -o $@

$(BINDIR)/worker: $(SRCDIR)/worker.c $(SHARED_OBJ)
	$(CC) $^ -o $@

$(BINDIR)/user: $(SRCDIR)/user.c $(SHARED_OBJ)
	$(CC) $^ -o $@

$(BINDIR)/ticket_erogator: $(SRCDIR)/ticket_erogator.c $(SHARED_OBJ)
	$(CC) $^ -o $@

# Clean target
clean:
	rm -rf $(BINDIR)

.PHONY: all clean