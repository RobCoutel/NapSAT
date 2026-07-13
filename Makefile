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

# Rules below add targets before `all`; pin the default goal explicitly so
# `make` with no arguments still builds the release binary.
.DEFAULT_GOAL := all

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
#
# NapSAT.a bundles SATSentinel.a's objects too, so that anything linking
# against NapSAT.a (e.g. Vampire) never needs to know SATSentinel exists as
# a separate archive. It also bundles static bz2/lzma (found via the
# compiler's own library search, so this works across distros without
# hardcoding a path), so consumers need no -lbz2/-llzma of their own either.
# GL and glfw can't be bundled the same way: GL is a runtime dispatch shim
# to the GPU driver with no static archive to speak of, and this distro's
# glfw3 package ships no static .a at all -- both stay as regular link
# dependencies (see the top-level CMakeLists.txt).
#
# Each merged archive's objects are extracted into their own scratch
# subdirectory and renamed with a prefix, since filenames can collide both
# across archives and within NapSAT/SATSentinel's own src trees (e.g.
# options.cpp, printer.cpp) and would otherwise collide as archive member
# names.
BZ2_STATIC := $(shell $(CC) -print-file-name=libbz2.a)
LZMA_STATIC := $(shell $(CC) -print-file-name=liblzma.a)
EXTERNAL_MERGE_DIR := $(BUILD_DIR)/.external-merge
$(BUILD_DIR)/$(TARGET_LIB): $(OBJS) $(SATSENTINEL_LIB)
	@test -f $(BZ2_STATIC) || { echo "libbz2.a not found (install libbz2-dev)"; exit 1; }
	@test -f $(LZMA_STATIC) || { echo "liblzma.a not found (install liblzma-dev)"; exit 1; }
	$(RM) $@
	$(RM) -r $(EXTERNAL_MERGE_DIR)
	$(MKDIR_P) $(EXTERNAL_MERGE_DIR)/satsentinel $(EXTERNAL_MERGE_DIR)/bz2 $(EXTERNAL_MERGE_DIR)/lzma
	cd $(EXTERNAL_MERGE_DIR)/satsentinel && ar x $(abspath $(SATSENTINEL_LIB))
	cd $(EXTERNAL_MERGE_DIR)/bz2 && ar x $(BZ2_STATIC)
	cd $(EXTERNAL_MERGE_DIR)/lzma && ar x $(LZMA_STATIC)
	for f in $(EXTERNAL_MERGE_DIR)/satsentinel/*.o; do mv "$$f" "$(EXTERNAL_MERGE_DIR)/satsentinel-$$(basename $$f)"; done
	for f in $(EXTERNAL_MERGE_DIR)/bz2/*.o; do mv "$$f" "$(EXTERNAL_MERGE_DIR)/bz2-$$(basename $$f)"; done
	for f in $(EXTERNAL_MERGE_DIR)/lzma/*.o; do mv "$$f" "$(EXTERNAL_MERGE_DIR)/lzma-$$(basename $$f)"; done
	ar rcs $@ $(OBJS) $(EXTERNAL_MERGE_DIR)/*.o
	$(RM) -r $(EXTERNAL_MERGE_DIR)

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
	build/NapSAT-tests

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

.PHONY: fuzz fuzz-ub perf-bench

# Metamorphic + cross-strategy fuzzing (see scripts/fuzz.py, KNOWN-ISSUES.md).
fuzz: debug
	python3 scripts/fuzz.py func

# Crash/UB fuzzing needs a sanitizer build; DBG_FLAGS uses `?=` so an
# exported override wins over the default and flows through the recursive
# SATSentinel sub-make too.
fuzz-ub:
	DBG_FLAGS="-O0 -g -g3 -gdwarf-2 -ftrapv -fsanitize=address,undefined -fno-sanitize-recover=all" $(MAKE) debug
	python3 scripts/fuzz.py ub

# Propagations/sec benchmark against tests/cnf/bench (see scripts/bench.py).
# Named perf-bench, not bench, to avoid clobbering the existing `bench`
# target above (renames the built binary to build/NapSAT-<commit-count> for
# manual A/B comparisons).
perf-bench: all
	python3 scripts/bench.py run tests/cnf/bench --out perf-bench.json

.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)
	$(RM) -r $(SATSENTINEL_BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
