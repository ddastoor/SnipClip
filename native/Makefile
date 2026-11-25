CC ?= gcc
PKG_CONFIG ?= pkg-config

CFLAGS_BASE := -std=c11 -O2 -Wall -Wextra -Iinclude -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 gio-2.0 x11)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 gio-2.0 x11) -lm

BUILD_DIR := build
APP_BIN := $(BUILD_DIR)/snipclip
TEST_BIN := $(BUILD_DIR)/test_snipclip_native
GTK_TEST_BIN := $(BUILD_DIR)/test_editor_gtk

APP_SRCS := \
	src/main.c \
	src/display_detect.c \
	src/geometry.c \
	src/capture_backend.c \
	src/overlay.c \
	src/editor_canvas.c \
	src/config_store.c \
	src/global_hotkey.c

APP_HDRS := $(wildcard include/*.h)

# Headless pure C11 unit suite: must stay GTK/GLib-free and link with -lm only
TEST_SRCS := \
	tests/test_native.c \
	src/display_detect.c \
	src/geometry.c \
	src/config_store.c

# Headless GTK 3 / Cairo editor canvas suite (run under xvfb-run)
GTK_TEST_SRCS := \
	tests/test_editor_gtk.c \
	src/editor_canvas.c \
	src/geometry.c

.PHONY: all clean test test-gtk

all: $(APP_BIN) $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(APP_BIN): $(APP_SRCS) $(APP_HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_BASE) $(GTK_CFLAGS) $(APP_SRCS) -o $(APP_BIN) $(GTK_LIBS)
	strip $(APP_BIN)

$(TEST_BIN): $(TEST_SRCS) $(APP_HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_BASE) $(TEST_SRCS) -o $(TEST_BIN) -lm

$(GTK_TEST_BIN): $(GTK_TEST_SRCS) $(APP_HDRS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_BASE) $(GTK_CFLAGS) $(GTK_TEST_SRCS) -o $(GTK_TEST_BIN) $(GTK_LIBS)

test: $(TEST_BIN)
	./$(TEST_BIN)

test-gtk: $(GTK_TEST_BIN)
	./$(GTK_TEST_BIN)

clean:
	rm -rf $(BUILD_DIR)
