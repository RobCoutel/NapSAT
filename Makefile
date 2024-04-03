EXEC = NapSAT-$(shell git rev-list --count --all)
LIB_NAME = sat
MAIN = main.cpp
TARGET_EXEC ?= $(LIB_NAME).a

CC := g++

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
TEST_DIRS ?= ./tests

SRCS := $(shell find $(SRC_DIRS) -name "*.cpp")
TEST_SRCS := $(shell find $(TEST_DIRS) -name "*.cpp")

OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DBG_OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%-dbg.o)
TEST_OBJS := $(TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJ := $(BUILD_DIR)/$(MAIN:.cpp=.o)
MAIN_OBJ_DBG := $(BUILD_DIR)/$(MAIN:.cpp=-dbg.o)

HEAD := $(shell find $(SRC_DIRS) -name "*.hpp")
TEST_HEAD := $(shell find $(TEST_DIRS) -name *.hpp)

DEPS := $(OBJS:.o=.d)
MODULES_DIR ?= ..
MODULES :=

INC_DIRS += ./include/ $(foreach D, $(MODULES), $(MODULES_DIR)/$(D)/include/)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS ?= $(INC_FLAGS) -MMD -MP -fPIC -std=c++17 -Wall --pedantic
REL_FLAGS ?= -O3 -DNDEBUG
DBG_FLAGS ?= -O0 -g -g3 -gdwarf-2 -ftrapv

# c source
$(BUILD_DIR)/%.o: %.cpp $(HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(REL_FLAGS) -c $< -o $@

$(BUILD_DIR)/%-dbg.o: %.cpp $(HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(DBG_FLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: %.cpp $(TEST_HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(DBG_FLAGS) -c $< -o $@

# library
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	ar rcs $@ $^

# release
$(BUILD_DIR)/$(EXEC): $(OBJS) $(MAIN_OBJ)
	$(CC) $(CFLAGS) $(REL_FLAGS) $^ -o $@

# debug
$(BUILD_DIR)/$(EXEC)-dbg: $(DBG_OBJS) $(MAIN_OBJ_DBG)
	$(CC) $(CFLAGS) $(DBG_FLAGS) $^ -o $@

# test
$(BUILD_DIR)/test: $(TEST_OBJS) $(DBG_OBJS)
	$(CC) $(CFLAGS) $(DBG_FLAGS) $^ -o $@

.PHONY: all

all: $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(EXEC)-dbg $(BUILD_DIR)/test

.PHONY: debug

debug: $(BUILD_DIR)/$(EXEC)-dbg

.PHONY: test

test: $(BUILD_DIR)/test


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
