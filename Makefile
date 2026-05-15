.PHONY: all clean build test format

# Default target: build using CMake (recommended)
all: build

# Build using CMake
build:
	rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)

# Clean build artifacts
clean:
	rm -rf build

# Run tests
test:
	cd build && ctest --output-on-failure

# Format code using clang-format
format:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Run the program (for quick testing)
run:
	./build/term-ime