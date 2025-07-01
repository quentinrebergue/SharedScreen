CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu99 -pedantic -Wvla -g -D_GNU_SOURCE -Iinclude
LDFLAGS = -luring

SRCDIR = src
OBJDIR = build
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))

TARGET = server

.PHONY: all clean

all: $(TARGET)

$(OBJDIR):
	mkdir -p $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(TARGET)
