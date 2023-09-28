EXEC = modulariT-SAT

CC := g++

BUILD_DIR ?= ./build
SRC_DIRS ?= ./src

SRCS := $(shell find $(SRC_DIRS) -name *.cpp)
OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%.o)
DBG_OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/%-dbg.o)
HEAD := $(shell find $(SRC_DIRS) -name *.hpp)
DEPS := $(OBJS:.o=.d)
MODULES_DIR ?= ..
MODULES :=

INC_DIRS += ./include/ $(foreach D, $(MODULES), $(MODULES_DIR)/$(D)/include/)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS ?= $(INC_FLAGS) -MMD -MP -fPIC -std=c++11 -Wall --pedantic
REL_FLAGS ?= -O3 -DNDEBUG
DBG_FLAGS ?= -O0 -g -g3 -gdwarf-2 -ftrapv -DDEBUG=1

# c source
$(BUILD_DIR)/%.o: %.cpp $(HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(REL_FLAGS) -c $< -o $@

$(BUILD_DIR)/%-dbg.o: %.cpp $(HEAD)
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) $(DBG_FLAGS) -c $< -o $@

# release
$(BUILD_DIR)/$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(REL_FLAGS) $^ -o $@

# debug
$(BUILD_DIR)/$(EXEC)-dbg: $(DBG_OBJS)
	$(CC) $(CFLAGS) $(DBG_FLAGS) $^ -o $@

.PHONY: all

all: $(BUILD_DIR)/$(EXEC) $(BUILD_DIR)/$(EXEC)-dbg

.PHONY: debug

debug: $(BUILD_DIR)/$(EXEC)-dbg


.PHONY: clean

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
