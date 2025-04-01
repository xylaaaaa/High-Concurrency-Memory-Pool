# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -g -pthread

# 目标文件和源文件
TARGET = UnitTest
SRC = UnitTest.cc
HEADERS = $(wildcard *.hpp)

# 默认目标
all: $(TARGET)

# 直接从源文件编译可执行文件
$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 清理规则
clean:
	rm -f $(TARGET)

# 声明伪目标
.PHONY: all clean
