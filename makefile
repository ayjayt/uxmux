CWARNFLAGS = -Wall -Wextra -Wpedantic -Wchkp -Wformat=2 -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused-parameter

CFLAGS = -fno-rtti -fno-builtin -Os -ffreestanding
CFLAGS += `pkg-config --cflags freetype2` `pkg-config --cflags libpng` -isystem lib/litehtml/include/

LDFLAGS = -s -v -nodefaultlibs -lc -ldl -lstdc++ -lgcc_s -Wl,-Map=build/uxmux.map,--cref,-t
LDFLAGS += -Llib/litehtml/ -llitehtml `pkg-config --libs freetype2` `pkg-config --libs libpng`

all:
	if [ -f "lib/litehtml/liblitehtml.a" ]
	then
	:
	else
		cd lib/litehtml && \
		cmake . && \
		make && \
		cd ../..
	fi

	g++ $(CWARNFLAGS) $(CFLAGS) -c main.cpp
	g++ -o build/uxmux main.o $(LDFLAGS)

extra:
	gcc -fpie -c extra.c -o extra.o
	gcc -o extra.elf -Wl,-pie,-E extra.o

clean_extra:
	rm extra.o
	rm extra.elf

clean_litehtml:
	cd lib/litehtml && \
	make clean && \
	cd ../..

clean:
	rm build/uxmux
	rm build/uxmux.map
	rm *.o

clean_all:
	cd lib/litehtml && \
	make clean && \
	cd ../..
	rm build/uxmux
	rm build/uxmux.map
	rm extra.elf
	rm *.o

cross_compile:
	export BASE_DIR=/root/Desktop
	export ZLIB_DIR=${BASE_DIR}/zlib/final
	export PNG_DIR=${BASE_DIR}/png/final
	export FREETYPE_DIR=${BASE_DIR}/freetype/final

	export TARGETMACH=arm-linux-gnueabihf
	export BUILDMACH=${MACHTYPE}
	export CROSS=arm-linux-gnueabihf
	export CC=${CROSS}-gcc
	export LD=${CROSS}-ld
	export AS=${CROSS}-as
	export CXX=${CROSS}-g++

	if [ -d "ZLIB_DIR" ] && [ -d "PNG_DIR" ] && [ -d "FREETYPE_DIR" ]
	then
		if [ -d "lib/litehtml/final/" ]
		then
		:
		else
			cd lib/litehtml/ && \
			cmake . && \
			make && \
		    mkdir final/ && \
			cp -r src/ final/src && \
			cp -r include/ final/include && \
			cp liblitehtml.a final/ && \
			cd ../..
		fi

		if [ -d "final/" ]
		then
		:
		else
		    mkdir final/
		fi

		$CXX -fno-rtti -fno-builtin -Os -ffreestanding -isystem ${ZLIB_DIR}/include -isystem ${PNG_DIR}/include/libpng16 -isystem ${FREETYPE_DIR}/include/freetype2 -isystem ${LITEHTML_DIR}/final/include -c main.cpp
		$CXX -o final/uxmux main.o -s -v -nodefaultlibs -lc -lm -ldl -lstdc++ -lgcc_s -Wl,-t,-static -L${LITEHTML_DIR}/final -llitehtml -L${FREETYPE_DIR}/lib -lfreetype -L${PNG_DIR}/lib -lpng16 -L${ZLIB_DIR}/lib -lz

		cp lib/litehtml/final/include/master.css final/
	else
		echo "FAILURE: Did you set up the cross-compiling toolchain?"
	fi

clean_cross:
	rm -r final/
	rm -r lib/litehtml/final/
