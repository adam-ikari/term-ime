CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lutil

SRC_DIR = src
BUILD_DIR = build

SRCS = $(SRC_DIR)/main.cpp \
       $(SRC_DIR)/terminal/pty.cpp \
       $(SRC_DIR)/terminal/screen.cpp \
       $(SRC_DIR)/terminal/parser.cpp \
       $(SRC_DIR)/ime/engine.cpp \
       $(SRC_DIR)/ime/pinyin.cpp \
       $(SRC_DIR)/ime/dict.cpp \
       $(SRC_DIR)/ui/renderer.cpp \
       $(SRC_DIR)/util/utf8.cpp

OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

TARGET = term-ime

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	@mkdir -p $(BUILD_DIR)/terminal $(BUILD_DIR)/ime $(BUILD_DIR)/ui $(BUILD_DIR)/util

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)