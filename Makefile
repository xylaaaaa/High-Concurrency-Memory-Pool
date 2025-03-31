# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11

# 目标文件和源文件
TARGET = test
SRC = test.cpp

# 编译规则
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 清理规则
clean:
	rm -f $(TARGET)

.PHONY: clean
