CC = gcc
CFLAGS = -Wall -g -fPIC -I$(INCLUDE_DIR)
LDFLAGS = -shared
TST_CFLAGS = $(CFLAGS) -DSIGTEST_TEST
TST_LDFLAGS = -g
CLI_CFLAGS = $(CFLAGS)
CLI_LDFLAGS = -g -L$(LIB_DIR) -lsigtest -Wl,-rpath,$(LIB_DIR)

SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin
LIB_DIR = $(BIN_DIR)/lib
TEST_DIR = test
LIB_TEST_DIR = test/lib
TST_BUILD_DIR = $(BUILD_DIR)/test
TEMPLATE_DIR = templates
RESOURCE_DIR = resources

SRCS = $(wildcard $(SRC_DIR)/*.c)
HOOKS_SRCS = $(wildcard $(SRC_DIR)/hooks/*.c)
CLI_SRC = $(SRC_DIR)/sigtest_cli.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(filter-out $(CLI_SRC) $(HOOKS_SRCS), $(SRCS)))
HOOKS_OBJS = $(patsubst $(SRC_DIR)/hooks/%.c, $(BUILD_DIR)/hooks/%.o, $(HOOKS_SRCS))
CLI_OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(CLI_SRC))
TST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TST_OBJS = $(patsubst $(TEST_DIR)/%.c, $(TST_BUILD_DIR)/%.o, $(TST_SRCS))
LIB_TST_SRCS = $(wildcard $(LIB_TEST_DIR)/*.c)
LIB_TST_OBJS = $(patsubst $(LIB_TEST_DIR)/%.c, $(TST_BUILD_DIR)/%.o, $(LIB_TST_SRCS))

HEADER = $(INCLUDE_DIR)/sigtest.h
LIB_TEST_HEADER = $(LIB_TEST_DIR)/math_utils.h

LIB_TARGET = $(LIB_DIR)/libstest.so
BIN_TARGET = $(BIN_DIR)/stest
TST_TARGET = $(TST_BUILD_DIR)/run_tests
# objectify
OBJECTIFY_TARGET = $(BIN_DIR)/objectify

INSTALL_LIB_DIR = /usr/lib
INSTALL_INCLUDE_DIR = /usr/include
INSTALL_BIN_DIR = /usr/bin

.PRECIOUS: $(TST_BUILD_DIR)/test_%

all: $(LIB_TARGET)

$(LIB_TARGET): $(OBJS)
	@mkdir -p $(LIB_DIR)
	$(CC) $(OBJS) -o $(LIB_TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(BUILD_DIR)/hooks/%.o: $(SRC_DIR)/hooks/%.c $(HEADER)
	@mkdir -p $(BUILD_DIR)/hooks
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(CLI_OBJ): $(CLI_SRC) $(HEADER)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CLI_CFLAGS) -c $< -o $@

$(TST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c $(HEADER)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -c $< -o $@

$(TST_BUILD_DIR)/%.o: $(LIB_TEST_DIR)/%.c $(HEADER) $(LIB_TEST_HEADER)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_CFLAGS) -I$(LIB_TEST_DIR) -c $< -o $@

$(OBJECTIFY_TARGET): tools/objectify.c $(HEADER)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(BIN_TARGET): $(LIB_TARGET) $(CLI_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CLI_OBJ) -o $(BIN_TARGET) $(CLI_LDFLAGS)

$(TST_TARGET): $(TST_OBJS) $(OBJS)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_OBJS) $(OBJS) -o $(TST_TARGET) $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_%: $(TST_BUILD_DIR)/test_%.o $(OBJS)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $< $(OBJS) -o $@ $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_hooks: $(TST_BUILD_DIR)/test_hooks.o $(OBJS) $(BUILD_DIR)/hooks/json_hooks.o
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_BUILD_DIR)/test_hooks.o $(OBJS) $(BUILD_DIR)/hooks/json_hooks.o -o $@ $(TST_LDFLAGS)

$(TST_BUILD_DIR)/test_lib: $(TST_BUILD_DIR)/test_lib.o $(TST_BUILD_DIR)/math_utils.o $(LIB_TARGET)
	@mkdir -p $(TST_BUILD_DIR)
	$(CC) $(TST_BUILD_DIR)/test_lib.o $(TST_BUILD_DIR)/math_utils.o -o $@ -L$(LIB_DIR) -lsigtest $(TST_LDFLAGS)

lib: $(LIB_TARGET) $(HEADER)

cli: $(BIN_TARGET)

objectify: $(OBJECTIFY_TARGET)

test_lib: $(TST_BUILD_DIR)/test_lib
	@$<

test_hooks: $(TST_BUILD_DIR)/test_hooks
	@$<

install: $(LIB_TARGET) $(HEADER) $(BIN_TARGET)
	sudo cp $(LIB_TARGET) $(INSTALL_LIB_DIR)/
	sudo cp $(INCLUDE_DIR)/sigtest.h $(INSTALL_INCLUDE_DIR)/
	sudo cp $(BIN_TARGET) $(INSTALL_BIN_DIR)/
	sudo ldconfig

build_%: $(TST_BUILD_DIR)/test_%
	@echo "Built $<"

build_test_%: $(TST_BUILD_DIR)/test_%
	@echo "Built $<"

test_%: $(TST_BUILD_DIR)/test_%
	@$<

suite: $(TST_TARGET)
	@$(TST_TARGET)

clean:
	find $(BUILD_DIR) -type f -delete
	find $(BIN_DIR) -type f -delete
clean-objectify:
	rm -rf $(SRC_DIR)/templates/*.ct $(RESOURCE_DIR)/*.ro

.PHONY: all clean clean-objectify lib cli install suite test_% build_% build_test_% test_lib test_hooks objectify