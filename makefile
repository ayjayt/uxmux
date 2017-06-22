CFLAGS = -Wall -Wextra -Wpedantic -O1 -Wchkp -Wformat=2 -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused-parameter
CFLAGS += `pkg-config --cflags freetype2` `pkg-config --cflags libpng` -isystem lib/litehtml/include/
LDFLAGS = -Llib/litehtml/ -llitehtml `pkg-config --libs freetype2` `pkg-config --libs libpng`

all:
	g++ $(CFLAGS) -c test.cpp test_container.cpp image_loader.cpp
	g++ -o build/test test.o test_container.o image_loader.o $(LDFLAGS)
