build:
	cmake -B build -G Ninja
	cmake --build build

test:
	cmake -B build -G Ninja
	cmake --build build --target cclock-geom-test
	./build/cclock-geom-test

run *args:
	cmake -B build -G Ninja
	cmake --build build
	./build/cclock {{args}}
