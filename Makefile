# Sigma-Test — Canonical Makefile
# Silent, safe, professional. Everything works exactly as you expect.

.SILENT:

CC = gcc
CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)
TST_CFLAGS = $(CFLAGS) -DSIGTEST_TEST
TST_CFLAGS += -Wno-unused-result  # Suppress "ignoring return value of 'malloc'"
WRAP_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=calloc -Wl,--wrap=realloc
LDFLAGS = -shared $(WRAP_LDFLAGS)
TST_LDFLAGS = -g $(WRAP_LDFLAGS)

# Directories
SRC_DIR       = src
INCLUDE_DIR   = include
BUILD_DIR     = build
BIN_DIR       = bin
LIB_DIR       = $(BIN_DIR)/lib
TEST_DIR      = test
TST_BUILD_DIR = $(BUILD_DIR)/test

HEADER = $(INCLUDE_DIR)/sigtest.h

# === Core sources (exclude CLI and hooks) — FIXED, NO OVERWRITE ===
ALL_SRCS   := $(wildcard $(SRC_DIR)/*.c)
CORE_SRCS  := $(filter-out $(SRC_DIR)/sigtest_cli.c,$(ALL_SRCS))
CORE_SRCS  := $(filter-out $(wildcard $(SRC_DIR)/hooks/*.c),$(CORE_SRCS))
OBJS       := $(CORE_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# === CLI ===
CLI_SRC    = $(SRC_DIR)/sigtest_cli.c
CLI_OBJ    = $(BUILD_DIR)/sigtest_cli.o
CLI_TARGET = $(BIN_DIR)/stest

# === Library ===
LIB_TARGET = $(LIB_DIR)/libstest.so
TST_OBJS := $(CORE_SRCS:$(SRC_DIR)/%.c=$(TST_BUILD_DIR)/%.o)

# === Build directories (created on demand) ===
$(BUILD_DIR)/hooks $(TST_BUILD_DIR) $(LIB_DIR) $(BIN_DIR):
	@mkdir -p $@

# === Compile core sources ===
# Library objects (with -fPIC for shared library)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER) | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

# Test objects (with test flags)
$(TST_BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER) | $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

# === Compile CLI ===
$(CLI_OBJ): $(CLI_SRC) $(HEADER) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# === Hook objects (built on demand) ===
$(BUILD_DIR)/hooks/%.o: $(SRC_DIR)/hooks/%.c $(HEADER) | $(BUILD_DIR)/hooks
	$(CC) $(TST_CFLAGS) -c $< -o $@

# === Shared library — THIS NOW WORKS ===
$(LIB_TARGET): $(OBJS) | $(LIB_DIR)
	$(CC) $(OBJS) -shared -o $@ $(LDFLAGS)

# === CLI binary (links against libstest.so) ===
$(CLI_TARGET): $(CLI_OBJ) $(LIB_TARGET) | $(BIN_DIR)
	$(CC) $< -o $@ -L$(LIB_DIR) -lstest -Wl,-rpath,$(LIB_DIR) $(TST_LDFLAGS)

# === Test object files ===
$(TST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c $(HEADER) | $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

# === Hook tests — PERFECT, WORKING, DO NOT TOUCH ===
$(TST_BUILD_DIR)/test_%_hooks: $(TST_BUILD_DIR)/test_%_hooks.o $(TST_OBJS) $(BUILD_DIR)/hooks/%_hooks.o | $(TST_BUILD_DIR)
	$(CC) $< $(TST_OBJS) $(BUILD_DIR)/hooks/$*_hooks.o -o $@ $(TST_LDFLAGS)

# === Generic tests ===
$(TST_BUILD_DIR)/test_%: $(TST_BUILD_DIR)/test_%.o $(TST_OBJS) | $(TST_BUILD_DIR)
	$(CC) $< $(TST_OBJS) -o $@ $(TST_LDFLAGS)

# === Default: `make` = build the core library only (your way) ===
all: lib
lib: $(LIB_TARGET)
	@echo "libstest.so built → $(LIB_TARGET)"

# === CLI target ===
cli: $(CLI_TARGET)
	@echo "CLI built → $(CLI_TARGET)"

# === Run tests ===
test_%_hooks: $(TST_BUILD_DIR)/test_%_hooks
	@$<
test_%: $(TST_BUILD_DIR)/test_%
	@$<

# === Full suite ===
suite: $(TST_BUILD_DIR)/run_tests
	@$<

# === Clean — SAFE, SILENT, NEVER TOUCHES bin/ ===
clean:
	find $(BUILD_DIR) -type f -delete 2>/dev/null || true
	@echo "Clean complete — bin/ preserved"

# === Never delete test binaries ===
.PRECIOUS: $(TST_BUILD_DIR)/test_% $(TST_BUILD_DIR)/test_%_hooks

.PHONY: lib cli clean test_% test_%_hooks suite