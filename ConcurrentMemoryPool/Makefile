# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -g

# 目标文件和源文件
TARGET = objectpool
SRC = ObjectPool.cc
OBJ = $(SRC:.cc=.o)
HEADERS = $(wildcard *.hpp)

# 默认目标
all: $(TARGET)

# 编译规则
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

# 生成目标文件
%.o: %.cc $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理规则
clean:
	rm -f $(TARGET) $(OBJ)

# 声明伪目标
.PHONY: all clean
