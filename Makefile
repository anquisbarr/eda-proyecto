all: build

build:
	mkdir -p build
	cd build && cmake .. && cmake --build .

run:
	./build/eda_proyecto_app

clean:
	rm -rf build 