.SILENT:

CC = gcc
CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)
LDFLAGS = -shared
TST_CFLAGS = $(CFLAGS) -DSIGTEST_TEST
TST_LDFLAGS = -g
CLI_CFLAGS = $(CFLAGS)
CLI_LDFLAGS = -g -L$(LIB_DIR) -lsigtest -Wl,-rpath,$(LIB_DIR)

# === MEMORY SAFETY — ALWAYS ON (in-house) ===
MEMORY_WRAP = -Wl,--wrap=malloc \
              -Wl,--wrap=free \
              -Wl,--wrap=calloc \
              -Wl,--wrap=realloc

TST_LDFLAGS += $(MEMORY_WRAP)
CLI_LDFLAGS += $(MEMORY_WRAP)

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = $(BIN_DIR)/lib
TEST_DIR = test
LIB_TEST_DIR = test/lib
TST_BUILD_DIR = $(BUILD_DIR)/test

SRCS = $(wildcard $(SRC_DIR)/*.c)
HOOKS_SRCS = $(wildcard $(SRC_DIR)/hooks/*.c)
CLI_SRC = $(SRC_DIR)/sigtest_cli.c

# ALL core objects (including memory.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(filter-out $(CLI_SRC) $(HOOKS_SRCS), $(SRCS)))
HOOKS_OBJS = $(patsubst $(SRC_DIR)/hooks/%.c, $(BUILD_DIR)/hooks/%.o, $(HOOKS_SRCS))
CLI_OBJ = $(BUILD_DIR)/sigtest_cli.o

TST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(TST_BUILD_DIR)/%.o, $(TST_SRCS))

HEADER = $(INCLUDE_DIR)/sigtest.h

LIB_TARGET = $(LIB_DIR)/libstest.so
BIN_TARGET = $(BIN_DIR)/stest
TST_TARGET = $(TST_BUILD_DIR)/run_tests

# memory.c is now part of normal SRCS → automatically in $(OBJS)
MEMORY_SRC = $(SRC_DIR)/memory.c

all: $(LIB_TARGET)

# Shared library — NO memory tracking (pure)
$(LIB_TARGET): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(CC) $(OBJS) -o $(LIB_TARGET) $(LDFLAGS)

# Object rules
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(BUILD_DIR)/hooks/%.o: $(SRC_DIR)/hooks/%.c $(HEADER)
	@mkdir -p $(BUILD_DIR)/hooks
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(MEMORY_OBJ): $(SRC_DIR)/memory.c $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(CLI_OBJ): $(CLI_SRC) $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(TST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c $(HEADER)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

# CLI — gets memory protection
$(BIN_TARGET): $(CLI_OBJ) $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CLI_OBJ) $(OBJS) -o $(BIN_TARGET) $(CLI_LDFLAGS)

# Full test suite — memory.o already in $(OBJS), wrap flags in TST_LDFLAGS
$(TST_TARGET): $(TST_OBJS) $(OBJS)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $^ -o $@ $(TST_LDFLAGS)

# Individual test binaries — same story
$(TST_BUILD_DIR)/test_%: $(TST_BUILD_DIR)/test_%.o $(OBJS) $(MEMORY_OBJ)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $< $(OBJS) $(MEMORY_OBJ) -o $@ $(TST_LDFLAGS)

# Special cases
$(TST_BUILD_DIR)/test_hooks: $(TST_BUILD_DIR)/test_hooks.o $(OBJS) $(BUILD_DIR)/hooks/json_hooks.o
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $^ -o $@ $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_lib: $(TST_BUILD_DIR)/test_lib.o $(TST_BUILD_DIR)/math_utils.o $(LIB_TARGET)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $^ -o $@ -L$(LIB_DIR) -lsigtest $(TST_LDFLAGS)

# Phonies
suite: $(TST_TARGET)
	@$(TST_TARGET)

test_%: $(TST_BUILD_DIR)/test_%
	@$<

cli: $(BIN_TARGET)
lib: $(LIB_TARGET)

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PRECIOUS: $(TST_BUILD_DIR)/test_%

.PHONY: all clean suite cli lib test_%