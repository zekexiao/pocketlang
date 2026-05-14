
##  Copyright (c) 2020-2021 Thakee Nathees
##  Distributed Under The MIT License

CC             = gcc
CXX            = g++
CFLAGS         = -fPIC
CXXFLAGS       = -fPIC -std=c++17
DEBUG_CFLAGS   = -D DEBUG -g3 -Og
RELEASE_CFLAGS = -g -O3
LDFLAGS        = -lm -ldl

TARGET_EXEC = pocket
TARGET_LIB = libpocketlang.a
BUILD_DIR   = ./build/

SRC_DIRS = ./cli/ ./src/core/ ./src/libs/
LIB_SRC_DIRS = ./src/core/ ./src/libs/
INC_DIRS = ./src/include/

BIN_DIR = bin/
OBJ_DIR = obj/
LIB_DIR = lib/

SRCS := $(foreach DIR,$(SRC_DIRS),$(wildcard $(DIR)*.c))
SRCS += $(foreach DIR,$(SRC_DIRS),$(wildcard $(DIR)*.cpp))
OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.cpp=.o)
DEPS := $(OBJS:.o=.d)

# Library sources exclude CLI main.c
LIB_SRCS := $(foreach DIR,$(LIB_SRC_DIRS),$(wildcard $(DIR)*.c))
LIB_SRCS += $(foreach DIR,$(LIB_SRC_DIRS),$(wildcard $(DIR)*.cpp))
LIB_SRCS += $(wildcard ./src/libs/thirdparty/cJSON/*.c)
LIB_SRCS += $(wildcard ./src/libs/thirdparty/cwalk/*.c)
LIB_OBJS := $(LIB_SRCS:.c=.o)
LIB_OBJS := $(LIB_OBJS:.cpp=.o)

INC_FLAGS := $(addprefix -I,$(INC_DIRS))
DEP_FLAGS  = -MMD -MP
CC_FLAGS   = $(INC_FLAGS) $(DEP_FLAGS) $(CFLAGS)
CXX_FLAGS  = $(INC_FLAGS) $(DEP_FLAGS) $(CXXFLAGS)

DEBUG_DIR     = $(BUILD_DIR)Debug/
DEBUG_TARGET  = $(DEBUG_DIR)$(BIN_DIR)$(TARGET_EXEC)
DEBUG_LIB     = $(DEBUG_DIR)$(LIB_DIR)$(TARGET_LIB)
DEBUG_OBJS   := $(addprefix $(DEBUG_DIR)$(OBJ_DIR), $(OBJS))
DEBUG_LIB_OBJS := $(addprefix $(DEBUG_DIR)$(OBJ_DIR), $(LIB_OBJS))

RELEASE_DIR     = $(BUILD_DIR)Release/
RELEASE_TARGET  = $(RELEASE_DIR)$(BIN_DIR)$(TARGET_EXEC)
RELEASE_LIB     = $(RELEASE_DIR)$(LIB_DIR)$(TARGET_LIB)
RELEASE_OBJS   := $(addprefix $(RELEASE_DIR)$(OBJ_DIR), $(OBJS))
RELEASE_LIB_OBJS := $(addprefix $(RELEASE_DIR)$(OBJ_DIR), $(LIB_OBJS))

.PHONY: debug release all clean lib-debug lib-release lib

# default; target if run as `make`
debug: $(DEBUG_TARGET) $(DEBUG_LIB)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(DEBUG_DIR)$(OBJ_DIR)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(DEBUG_CFLAGS) -c $< -o $@

$(DEBUG_DIR)$(OBJ_DIR)%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(DEBUG_CFLAGS) -c $< -o $@

release: $(RELEASE_TARGET) $(RELEASE_LIB)

$(RELEASE_TARGET): $(RELEASE_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(RELEASE_DIR)$(OBJ_DIR)%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CC_FLAGS) $(RELEASE_CFLAGS) -c $< -o $@

$(RELEASE_DIR)$(OBJ_DIR)%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) $(RELEASE_CFLAGS) -c $< -o $@

# Static library targets
lib-debug: $(DEBUG_LIB)

$(DEBUG_LIB): $(DEBUG_LIB_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^
	@mkdir -p $(DEBUG_DIR)include/
	@cp -r $(INC_DIRS)* $(DEBUG_DIR)include/

lib-release: $(RELEASE_LIB)

$(RELEASE_LIB): $(RELEASE_LIB_OBJS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^
	@mkdir -p $(RELEASE_DIR)include/
	@cp -r $(INC_DIRS)* $(RELEASE_DIR)include/

lib: lib-debug lib-release

all: debug release lib

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
