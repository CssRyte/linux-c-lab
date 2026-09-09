CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11 -g

SRCDIR = src
CHDIR = challenges
BINDIR = bin

DEMO_SRCS = $(wildcard $(SRCDIR)/*.c)
DEMO_BINS = $(patsubst $(SRCDIR)/%.c,$(BINDIR)/%,$(DEMO_SRCS))

CHALLENGE_SRCS = $(wildcard $(CHDIR)/ch*.c)
CHALLENGE_BINS = $(patsubst $(CHDIR)/%.c,$(BINDIR)/%,$(CHALLENGE_SRCS))

HELPER_BIN = $(BINDIR)/worker

all: $(DEMO_BINS) $(CHALLENGE_BINS) $(HELPER_BIN)

$(DEMO_BINS): $(BINDIR)/%: $(SRCDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(CHALLENGE_BINS): $(BINDIR)/%: $(CHDIR)/%.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(HELPER_BIN): $(CHDIR)/worker.c | $(BINDIR)
	$(CC) $(CFLAGS) $< -o $@

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR) build

.PHONY: all clean