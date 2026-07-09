EXEC = NapSAT
LIB_NAME = NapSAT
MAIN = main.cpp
TARGET_LIB ?= $(LIB_NAME).a

CC := g++

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src
TEST_DIRS ?= ./tests
SATSENTINEL_DIR ?= ./SATSentinel
SATSENTINEL_BUILD_DIR := $(SATSENTINEL_DIR)/build
SATSENTINEL_LIB := $(SATSENTINEL_BUILD_DIR)/SATSentinel.a

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

BUILD_MODE ?= release
GUI ?= 0

INC_DIRS += ./include/ $(SATSENTINEL_DIR)/include/ $(SATSENTINEL_DIR)/src/ $(foreach D, $(MODULES), $(MODULES_DIR)/$(D)/include/)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
LINK_FLAGS := -llzma -lbz2
ifeq ($(GUI),1)
  LINK_FLAGS += -lglfw -lGL -ldl -lpthread
endif
TEST_LINK_FLAGS := -lCatch2Main -lCatch2

CFLAGS ?= $(INC_FLAGS) -MMD -MP -fPIC -std=c++20 -Wall --pedantic
REL_FLAGS ?= -O3 -DNDEBUG
DBG_FLAGS ?= -O0 -g -g3 -gdwarf-2 -ftrapv

# Fingerprint of the flags that affect compilation (optimization/debug
# flags, defines, ...). Made a prerequisite of every .o below so that
# switching between release/debug/tests forces a rebuild of the affected
# objects instead of silently reusing stale ones compiled with different
# flags (e.g. NDEBUG toggling invariant checks in custom-assert.hpp).
BUILD_FLAGS := GUI=$(GUI) REL_FLAGS=$(REL_FLAGS) CFLAGS=$(CFLAGS)
FLAGS_FILE := $(BUILD_DIR)/.build-flags

# c source
$(BUILD_DIR)/%.o: %.cpp $(HEAD) $(FLAGS_FILE)
	$(MKDIR_P) $(dir $@)
	$(CC) -c $< -o $@ $(REL_FLAGS) $(CFLAGS)

$(FLAGS_FILE): FORCE
	@$(MKDIR_P) $(dir $@)
	@echo '$(BUILD_FLAGS)' | cmp -s - $@ || echo '$(BUILD_FLAGS)' > $@

.PHONY: FORCE
FORCE:

# release
$(BUILD_DIR)/$(EXEC): $(OBJS) $(MAIN_OBJ) $(SATSENTINEL_LIB)
	$(CC) $^ -o $@ $(CFLAGS) $(REL_FLAGS) $(LINK_FLAGS)

# library
# Remove the archive before rebuilding: `ar rcs` never drops members that
# are no longer part of $^, so a stale .a could otherwise keep referencing
# objects built with a previous, incompatible set of flags.
$(BUILD_DIR)/$(TARGET_LIB): $(OBJS)
	$(RM) $@
	ar rcs $@ $^

$(SATSENTINEL_LIB):
	$(MAKE) -C $(SATSENTINEL_DIR) lib BUILD_MODE=$(BUILD_MODE) GUI=$(GUI)

.PHONY: SATSentinel

SATSentinel:
	$(MAKE) -C $(SATSENTINEL_DIR) lib BUILD_MODE=$(BUILD_MODE) GUI=$(GUI)

$(SATSENTINEL_LIB): SATSentinel

# tests
tests: REL_FLAGS = $(DBG_FLAGS) $(TEST_LINK_FLAGS)
tests: $(OBJS) $(TEST_OBJS) $(SATSENTINEL_LIB)
	$(CC) $^ -o $(BUILD_DIR)/NapSAT-tests $(CFLAGS) $(DBG_FLAGS) $(LINK_FLAGS) $(TEST_LINK_FLAGS)

.PHONY: debug

lib: $(BUILD_DIR)/$(TARGET_LIB)

all: $(BUILD_DIR)/$(TARGET_LIB)
all: $(BUILD_DIR)/$(EXEC)

debug: REL_FLAGS = $(DBG_FLAGS)
debug: $(BUILD_DIR)/$(TARGET_LIB)
debug: $(BUILD_DIR)/$(EXEC)
debug: BUILD_MODE = debug

.PHONY: install install-gui install-test
install:
	apt-get install liblzma libbz2
	git submodule update --init --recursive

install-gui:
	$(MAKE) -C $(SATSENTINEL_DIR) install-gui

install-test:
	apt-get install catch2

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
	$(RM) -r $(SATSENTINEL_BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
