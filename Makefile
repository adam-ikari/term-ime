.PHONY: all clean build test format run

# Default target: build using CMake
all: build

# Build using CMake
build:
	rm -rf build && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc)

# Clean build artifacts
clean:
	rm -rf build

# Run tests
test:
	./build/term-ime-tests

# Format code using clang-format
format:
	find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i

# Run the program (for quick testing)
run:
	./build/term-ime