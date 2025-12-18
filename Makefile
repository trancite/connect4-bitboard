TARGET_NAME = connectK

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

EXT =
RM = rm -f
FixPath = $1
MKDIR = mkdir -p $(OBJ_DIR)
RUN_CMD = ./$(EXEC)
PIE_FLAGS = -fno-PIE -no-pie

ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
    EXT = .exe
    RM = del /Q /F
    FixPath = $(subst /,\,$1)
    MKDIR = if not exist $(subst /,\,$(OBJ_DIR)) mkdir $(subst /,\,$(OBJ_DIR))
    RUN_CMD = $(EXEC)
    PIE_FLAGS = -fno-PIE -no-pie
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        PLATFORM = macOS
        PIE_FLAGS =
    else
        PLATFORM = Linux
    endif
endif

EXEC = $(TARGET_NAME)$(EXT)
CC = gcc

CFLAGS = -Wall -Wextra -O3 -std=c11 -march=native -flto \
         -I$(INC_DIR) $(PIE_FLAGS) -MMD -MP

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)


all: $(EXEC)
	@echo "--- Compilation finished successfully $(PLATFORM) ---"

$(EXEC): $(OBJS)
	@echo [LINK] Creating executable $(EXEC)...
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR)
	@echo [CC] Compiling $<...
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: clean run

clean:
	@echo Cleaning temporary files...
	$(RM) $(call FixPath, $(OBJ_DIR)/*.o)
	$(RM) $(call FixPath, $(OBJ_DIR)/*.d)
	$(RM) $(call FixPath, $(EXEC))

run: $(EXEC)
	$(RUN_CMD)
