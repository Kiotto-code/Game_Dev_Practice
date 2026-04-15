APPNAME            ?= Game
EXP             = EXPGame
CXX             = g++
RM              = rm -rf
MKDIR           = mkdir -p


# Directories
RAYLIB_DIR      = ./external/raylib
RAYLIB_LIB      = $(RAYLIB_DIR)/libraylib.a
ENTT_DIR        = ./external/entt
SRCS_PATH       ?= ./selfpractice
# SRCS_PATH       = ./srcs
OBJS_PATH       = ./objs
EXP_SRCS_PATH   = ./src-experiment
EXP_OBJS_PATH   = ./objsexp

# Compiler flags
BASE_FLAGS      = -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -MMD -MP
DEBUG_FLAGS     = -g3 -fsanitize=address
RELEASE_FLAGS   = -O3 -flto -DNDEBUG
CXXFLAG         = $(BASE_FLAGS) $(DEBUG_FLAGS)  # Default to debug

# INCLUDES        = -I./includes -I$(ENTT_DIR) -I$(RAYLIB_DIR)
INCLUDES        = -I"external/raylib" -I"external/entt"
LDFLAGS         = -L$(RAYLIB_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Source files
SRCS            = $(shell find $(SRCS_PATH) -name "*.cpp")
EXP_SRCS        = $(shell find $(EXP_SRCS_PATH) -name "*.cpp")

# Object files
OBJS            = $(patsubst $(SRCS_PATH)/%.cpp, $(OBJS_PATH)/%.o, $(SRCS))
EXP_OBJS        = $(patsubst $(EXP_SRCS_PATH)/%.cpp, $(EXP_OBJS_PATH)/%.o, $(EXP_SRCS))

# Dependencies
DEPS            = $(OBJS:.o=.d) $(EXP_OBJS:.o=.d)

# Colors
GREEN           = \033[0;32m
YELLOW          = \033[0;33m
RED             = \033[0;31m
RESET           = \033[0m

.PHONY: all clean fclean re run help debug release exp runexp raylib

# Default target
all: $(APPNAME)

# Main executable
$(APPNAME): $(RAYLIB_LIB) $(OBJS)
	@echo "$(YELLOW)Linking $@...$(RESET)"
	$(CXX) $(CXXFLAG) $(OBJS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)✓ Built $@ successfully!$(RESET)"

# Experiment executable
exp: $(EXP)

$(EXP): $(RAYLIB_LIB) $(EXP_OBJS)
	@echo "$(YELLOW)Linking $@...$(RESET)"
	$(CXX) $(CXXFLAG) $(EXP_OBJS) -o $@ $(LDFLAGS)
	@echo "$(GREEN)✓ Built $@ successfully!$(RESET)"

# Build raylib
$(RAYLIB_LIB):
	@echo "$(YELLOW)Building Raylib...$(RESET)"
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP
	@echo "$(GREEN)✓ Raylib built$(RESET)"

# Pattern rules for object files
$(OBJS_PATH)/%.o: $(SRCS_PATH)/%.cpp | $(OBJS_PATH)
	@echo "$(YELLOW)Compiling $<...$(RESET)"
	$(MKDIR) $(@D)
	$(CXX) $(CXXFLAG) $(INCLUDES) -c $< -o $@

$(EXP_OBJS_PATH)/%.o: $(EXP_SRCS_PATH)/%.cpp | $(EXP_OBJS_PATH)
	@echo "$(YELLOW)Compiling $<...$(RESET)"
	$(MKDIR) $(@D)
	$(CXX) $(CXXFLAG) $(INCLUDES) -c $< -o $@

# Create object directories
$(OBJS_PATH) $(EXP_OBJS_PATH):
	$(MKDIR) $@

# Include dependency files
-include $(DEPS)

# Clean targets
clean:
	@echo "$(YELLOW)Cleaning object files...$(RESET)"
	$(RM) $(OBJS_PATH) $(EXP_OBJS_PATH)
	@echo "$(GREEN)✓ Clean complete$(RESET)"

fclean: clean
	@echo "$(YELLOW)Cleaning executables...$(RESET)"
	$(RM) $(APPNAME) $(EXP)
	@echo "$(GREEN)✓ Full clean complete$(RESET)"

re: fclean all

# Build and run
run: all
	@echo "$(YELLOW)Running $(APPNAME)...$(RESET)"
	./$(APPNAME)

runexp: exp
	@echo "$(YELLOW)Running $(EXP)...$(RESET)"
	./$(EXP)

# Build variants
debug: CXXFLAG = $(BASE_FLAGS) $(DEBUG_FLAGS)
debug: re

release: CXXFLAG = $(BASE_FLAGS) $(RELEASE_FLAGS)
release: re

# Build raylib only
raylib: $(RAYLIB_LIB)

# Help target
help:
	@echo "$(GREEN)Available targets:$(RESET)"
	@echo "  all      : Build $(APPNAME) (default)"
	@echo "  exp      : Build $(EXP)"
	@echo "  debug    : Build with debug flags"
	@echo "  release  : Build with optimization flags"
	@echo "  clean    : Remove object files"
	@echo "  fclean   : Remove object files and executables"
	@echo "  re       : Clean and rebuild"
	@echo "  run      : Build and run $(APPNAME)"
	@echo "  runexp   : Build and run $(EXP)"
	@echo "  raylib   : Build raylib only"
	@echo "  help     : Show this help message"