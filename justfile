build:
	cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja -B build
	cmake --build build

run:
	./build/cclock
