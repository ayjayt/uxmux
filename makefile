CWARNFLAGS = -Wall -Wextra -Wpedantic -Wchkp -Wformat=2 -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused-parameter

CFLAGS = -fno-rtti -fno-builtin -Os -ffreestanding
CFLAGS += `pkg-config --cflags freetype2` `pkg-config --cflags libpng` -isystem lib/litehtml/include/

LDFLAGS = -s -v -nodefaultlibs -lc -ldl -lstdc++ -lgcc_s -Wl,-Map=build/uxmux.map,--cref,-t
LDFLAGS += -Llib/litehtml/ -llitehtml `pkg-config --libs freetype2` `pkg-config --libs libpng`

all:
	g++ $(CWARNFLAGS) $(CFLAGS) -c main.cpp
	g++ -o build/uxmux main.o $(LDFLAGS)

litehtml:
	cd lib/litehtml && \
	cmake . && \
	make && \
	cd ../..

extra:
	gcc -fpie -c extra.c -o extra.o
	gcc -o extra.elf -Wl,-pie,-E extra.o

clean_extra:
	rm extra.elf

clean:
	rm build/uxmux
	rm build/uxmux.map
	rm *.o
