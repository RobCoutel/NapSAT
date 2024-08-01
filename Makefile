EXEC = NapSAT
LIB_NAME = NapSAT
MAIN = main.cpp
TARGET_LIB ?= $(LIB_NAME).a

CC := clang++

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
TEST_DIRS ?= ./tests

SRCS := $(shell find $(SRC_DIRS) -name "*.cpp")
TEST_SRCS := $(shell find $(TEST_DIRS) -name "*.cpp")

OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:%.cpp=$(BUILD_DIR)/%.o)
MAIN_OBJ := $(BUILD_DIR)/$(MAIN:.cpp=.o)

HEAD := $(shell find $(SRC_DIRS) -name "*.hpp")
TEST_HEAD := $(shell find $(TEST_DIRS) -name *.hpp)

DEPS := $(OBJS:.o=.d)
MODULES_DIR ?= ..
MODULES :=

INC_DIRS += ./include/ $(foreach D, $(MODULES), $(MODULES_DIR)/$(D)/include/)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LINK_FLAGS := -llzma
TEST_LINK_FLAGS := -I /usr/include/catch2/catch.hpp

CFLAGS ?= $(INC_FLAGS) -MMD -MP -fPIC -std=c++17 -Wall --pedantic
REL_FLAGS ?= -O3 -DNDEBUG
DBG_FLAGS ?= -O0 -g -g3 -gdwarf-2 -ftrapv

# c source
$(BUILD_DIR)/%.o: %.cpp $(HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) -c $< -o $@ $(REL_FLAGS) $(CFLAGS)

# release
$(BUILD_DIR)/$(EXEC): $(OBJS) $(MAIN_OBJ)
	$(CC) $^ -o $@ $(CFLAGS) $(REL_FLAGS) $(LINK_FLAGS)

# library
$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	ar rcs $@ $^

# tests
tests: REL_FLAGS = $(DBG_FLAGS)
tests: $(OBJS) $(TEST_OBJS)
	$(CC) -o $(BUILD_DIR)/NapSAT-tests $(OBJS) $(TEST_OBJS) $(CFLAGS) $(DBG_FLAGS) $(LINK_FLAGS) $(TEST_LINK_FLAGS)

.PHONY: debug

lib: $(BUILD_DIR)/$(TARGET_LIB)

all: $(BUILD_DIR)/$(TARGET_LIB)
all: $(BUILD_DIR)/$(EXEC)

debug: REL_FLAGS = $(DBG_FLAGS)
debug: $(BUILD_DIR)/$(TARGET_LIB)
debug: $(BUILD_DIR)/$(EXEC)

.PHONY: install
	sudo apt-get install liblzma-dev

install-test:
	sudo apt-get install catch2

.PHONY: bench

COMMIT_COUNT := $(shell git rev-list --count --all)
UNCOMMITTED_CHANGES := $(shell git status --porcelain)

ifneq ($(UNCOMMITTED_CHANGES),)
  COMMIT_COUNT := $(shell echo $$(($(COMMIT_COUNT) + 1)))
endif

bench: $(BUILD_DIR)/$(EXEC)
bench:
	mv $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(EXEC)-$(COMMIT_COUNT)

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
