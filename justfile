build:
	cmake -B build -G Ninja
	cmake --build build

run *args:
	cmake -B build -G Ninja
	cmake --build build
	./build/cclock {{args}}
