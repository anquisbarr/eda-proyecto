all: build

build:
	mkdir -p build
	cd build && cmake .. && cmake --build .

run:
	./build/http_server

clean:
	rm -rf build

.PHONY: all build run clean 