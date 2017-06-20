FLAGS = -Wall -Wextra -Wpedantic -O1 -Wchkp -Wformat=2 -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused-parameter

all:
	g++ $(FLAGS) -c -isystem lib/litehtml/include/ test.cpp test_container.cpp
	g++ -o build/test test.o test_container.o -Llib/litehtml/ -llitehtml
