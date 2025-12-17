TARGET_NAME = connect4

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj

ifeq ($(OS),Windows_NT)
    EXT = .exe
    RM = del /Q /F
    FixPath = $(subst /,\,$1)
    MKDIR = if not exist $(subst /,\,$(OBJ_DIR)) mkdir $(subst /,\,$(OBJ_DIR))
    RUN_CMD = $(EXEC)
else
    EXT =
    RM = rm -f
    FixPath = $1
    MKDIR = mkdir -p $(OBJ_DIR)
    RUN_CMD = ./$(EXEC)
endif

EXEC = $(TARGET_NAME)$(EXT)

CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c11 -march=native -flto \
         -I$(INC_DIR) -fno-PIE -no-pie -MMD -MP

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

all: $(EXEC)

$(EXEC): $(OBJS)
	@echo [LINK] Creando ejecutable $(EXEC)...
	$(CC) $(CFLAGS) $(OBJS) -o $(EXEC)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR)
	@echo [CC] Compilando $<...
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

.PHONY: clean run

clean:
	@echo Limpiando archivos temporales...
	$(RM) $(call FixPath, $(OBJ_DIR)/*.o)
	$(RM) $(call FixPath, $(OBJ_DIR)/*.d)
	$(RM) $(call FixPath, $(EXEC))

run: $(EXEC)
	$(RUN_CMD)
