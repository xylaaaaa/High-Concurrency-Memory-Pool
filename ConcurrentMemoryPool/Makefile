# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -g -pthread

# 目标文件和源文件
BUILD_DIR = build
TARGET = $(BUILD_DIR)/UnitTest
SRCS = UnitTest.cc
HEADERS = $(wildcard *.hpp)

# 默认目标
all: $(TARGET)

# 直接从源文件编译可执行文件
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

# 清理规则
clean:
	rm -rf $(BUILD_DIR)
	rm -f UnitTest
	rm -rf UnitTest.dSYM

# 声明伪目标
.PHONY: all clean
