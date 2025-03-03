CC = gcc
SRCDIR = src
BINDIR = bin
CFLAGS = -g -Wvla -Wextra -Werror -Wall -pedantic

# Targets
TARGETS = $(BINDIR)/director $(BINDIR)/worker $(BINDIR)/user $(BINDIR)/ticket_erogator $(BINDIR)/add_user
SHARED_OBJ = $(BINDIR)/shared_function.o

# Default target
all: $(BINDIR) $(SHARED_OBJ) $(TARGETS)

# Create bin directory
$(BINDIR):
	mkdir -p $(BINDIR)

# Compile shared object
$(SHARED_OBJ): $(SRCDIR)/shared_function.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile binaries
$(BINDIR)/director: $(SRCDIR)/director.c $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BINDIR)/worker: $(SRCDIR)/worker.c $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BINDIR)/user: $(SRCDIR)/user.c $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

$(BINDIR)/ticket_erogator: $(SRCDIR)/ticket_erogator.c $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

# New target for add_user with pthread and realtime extensions
$(BINDIR)/add_user: $(SRCDIR)/add_user.c $(SHARED_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ -lrt -pthread

# Clean
clean:
	rm -rf $(BINDIR)

.PHONY: all clean