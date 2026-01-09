CXX = g++
CXXFLAGS = -Wall -Wextra -O3 -g -march=native -std=c++17 -Iinclude -pthread
SRC_DIR = src
OBJ_DIR = obj

# Find all .cpp files in src and subdirectories
SRCS = $(shell find $(SRC_DIR) -name "*.cpp")
# Map .cpp paths to .o paths (flattening structure or preserving? simpler to flatten for obj dir)
# Using generic rule requires preserving or careful handling.
# Let's flatten for simplicity in this script, or keep structure.
# Flattening: src/opt/foo.cpp -> obj/foo.o. Warning: name collisions.
# Safer: obj/src/opt/foo.o.

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TARGET = clercx

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to compile .cpp to .o, preserving directory structure in obj
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean