CC        := gcc
PKGCONFIG := pkg-config

PKG_DEPS  := glib-2.0 gio-2.0 gobject-2.0 libportal gdk-pixbuf-2.0

CFLAGS    := -std=gnu11 -Wall -Wextra -pthread -g \
             $(shell $(PKGCONFIG) --cflags $(PKG_DEPS)) \
             -Iinclude

LDFLAGS   := $(shell $(PKGCONFIG) --libs $(PKG_DEPS)) -luring

SRC       := src/main.c src/network.c src/png_converter.c src/screenshot.c
OBJ       := $(SRC:.c=.o)

TARGET    := client

all: $(TARGET)

# Lien final : on lie les .o pour produire l'exécutable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Règle générique pour compiler les .c en .o
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean

