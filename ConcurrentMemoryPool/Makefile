# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -g -pthread
BENCH_CXXFLAGS = -Wall -std=c++11 -O3 -DNDEBUG -pthread -I.

# 目标文件和源文件
BUILD_DIR = build
TARGET = $(BUILD_DIR)/UnitTest
BENCH_TARGET = $(BUILD_DIR)/allocator_bench
DEMO_TARGET = $(BUILD_DIR)/allocator_demo
SRCS = UnitTest.cc
BENCH_SRCS = bench/allocator_bench.cc
DEMO_SRCS = examples/allocator_integration_demo.cc
HEADERS = $(wildcard *.hpp)

# 默认目标
all: $(TARGET)

bench: $(BENCH_TARGET)

demo: $(DEMO_TARGET)

# 直接从源文件编译可执行文件
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TARGET): $(SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

$(BENCH_TARGET): $(BENCH_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(BENCH_CXXFLAGS) -o $(BENCH_TARGET) $(BENCH_SRCS)

$(DEMO_TARGET): $(DEMO_SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. -o $(DEMO_TARGET) $(DEMO_SRCS)

# 清理规则
clean:
	rm -rf $(BUILD_DIR)
	rm -f UnitTest
	rm -rf UnitTest.dSYM

# 声明伪目标
.PHONY: all bench demo clean
