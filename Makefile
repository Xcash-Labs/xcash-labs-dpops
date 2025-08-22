# Color print variables
COLOR_PRINT_RED ?= "\033[1;31m"
COLOR_PRINT_GREEN ?= "\033[1;32m"
END_COLOR_PRINT ?= "\033[0m"

# List the current file count and how many files total it is building
ifndef PRINT_CURRENT_FILE
TOTAL_FILES := $(shell $(MAKE) $(MAKECMDGOALS) --no-print-directory -nrRf $(firstword $(MAKEFILE_LIST)) PRINT_CURRENT_FILE="COUNTTHIS" | grep -c "COUNTTHIS")
N := x
CURRENT_FILE = $(words $N)$(eval N := x $N)
PRINT_CURRENT_FILE = echo -ne $(COLOR_PRINT_GREEN)"\r Currently Building File" $(CURRENT_FILE) "Out Of" $(TOTAL_FILES)":"$(END_COLOR_PRINT)
endif

# Binary name
TARGET_BINARY ?= xcash-dpops

# Build directory
BUILD_DIR ?= ./build

# Source directory
SRC_DIRS ?= ./src

# Find all source files (only .c files)
SRCS := $(shell find $(SRC_DIRS) -name "*.c")

# Create object files for each source file
OBJS := $(SRCS:%.c=$(BUILD_DIR)/%.o)

# Dependency files for each object file
DEPS := $(OBJS:.o=.d)

# Include dependency files if they exist
-include $(DEPS)

# Include directories
INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# MongoDB include directories
#MongoDB_INC_DIRS := -I/usr/local/include/libbson-1.0 -I/usr/local/include/libmongoc-1.0

# Compiler flags
#CFLAGS ?= $(INC_FLAGS) $(MongoDB_INC_DIRS) -MMD -MP -Wall -Wextra -Wstrict-prototypes -Wcast-qual -Wfloat-equal -Wundef -Wshadow -Wcast-align -Wstrict-overflow -Wdouble-promotion -fexceptions -pie -fPIE -Wl,dynamicbase -Wl,nxcompat
CFLAGS ?= $(INC_FLAGS) -MMD -MP -Wall -Wextra -Wstrict-prototypes -Wcast-qual -Wfloat-equal -Wundef -Wshadow -Wcast-align -Wstrict-overflow -Wdouble-promotion -fexceptions -pie -fPIE -Wl,dynamicbase -Wl,nxcompat


# Linker flags
LDFLAGS ?= -lmongoc-1.0 -lbson-1.0 -lresolv -lpthread -l:libcrypto.so.3 -lcurl -lcjson

# Build configurations
debug: CFLAGS += -g -fno-stack-protector
release: CFLAGS += -O3
release_seed: CFLAGS += -O3 -DSEED_NODE_ON
optimized: CFLAGS += -march=native -O3
analyze: CFLAGS += -g -Og -fsanitize=address -fsanitize=undefined
analyze: LDFLAGS += -fsanitize=address -fsanitize=undefined
analyzethreads: CFLAGS += -g -fsanitize=thread
analyzethreads: LDFLAGS += -fsanitize=thread

# Build all C object files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(PRINT_CURRENT_FILE) $@
	@$(CC) $(CFLAGS) -c $< -o $@

# Ensure `debug`, `release`, and `optimized` target the same binary
.PHONY: debug release optimized analyze analyzethreads release_seed clean

debug: $(BUILD_DIR)/$(TARGET_BINARY)
release: $(BUILD_DIR)/$(TARGET_BINARY)
release_seed: $(BUILD_DIR)/$(TARGET_BINARY)
optimized: $(BUILD_DIR)/$(TARGET_BINARY)
analyze: $(BUILD_DIR)/$(TARGET_BINARY)
analyzethreads: $(BUILD_DIR)/$(TARGET_BINARY)

# Link the target binary
$(BUILD_DIR)/$(TARGET_BINARY): $(OBJS)
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "\n" $(COLOR_PRINT_GREEN)$(TARGET_BINARY) "Has Been Built Successfully"$(END_COLOR_PRINT)

# Clean build artifacts
clean:
	@$(RM) -r $(BUILD_DIR)
	@echo $(COLOR_PRINT_RED) "Removed" $(BUILD_DIR)$(END_COLOR_PRINT)