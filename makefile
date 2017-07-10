SHELL := /bin/bash

CWARNFLAGS = -Wall -Wextra -Wpedantic -Wchkp -Wformat=2 -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wlogical-op -Wmissing-declarations -Wmissing-include-dirs -Wnoexcept -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wshadow -Wsign-conversion -Wsign-promo -Wstrict-null-sentinel -Wstrict-overflow=5 -Wswitch-default -Wundef -Wno-unused-parameter

CFLAGS = -fno-rtti -fno-builtin -Os -ffreestanding
CFLAGS += `pkg-config --cflags freetype2` `pkg-config --cflags libpng` -isystem lib/litehtml/include/

LDFLAGS = -s -v -nodefaultlibs -lc -ldl -lstdc++ -lgcc_s -Wl,-t
LDFLAGS += -Llib/litehtml/ -llitehtml `pkg-config --libs freetype2` `pkg-config --libs libpng`

all:
	if ( \
		if [ -f "lib/litehtml/liblitehtml.a" ] ; then : ; \
		else \
			cd lib/litehtml && \
			cmake . && \
			make && \
			cd ../.. ; \
		fi ; \
		g++ $(CWARNFLAGS) $(CFLAGS) -c main.cpp ; \
		g++ -o build/uxmux main.o -Wl,-Map=build/uxmux.map,--cref $(LDFLAGS) \
	) ; then : ; else echo "FAILURE: Running 'make clean_all && make' may solve the problem." ; fi

extra:
	gcc -fpie -c extra.c -o extra.o
	gcc -o extra.elf -Wl,-pie,-E extra.o



clean_extra:
	if [ -f extra.o ] ; then rm extra.o ; fi
	if [ -f extra.elf ] ; then rm extra.elf ; fi

clean_litehtml:
	cd lib/litehtml && \
	(make clean || :) && \
	if [ -f "CMakeCache.txt" ] ; then rm CMakeCache.txt ; fi && \
	if [ -f "cmake_install.cmake" ] ; then rm cmake_install.cmake ; fi && \
	if [ -d "CMakeFiles/" ] ; then rm -r CMakeFiles/ ; fi && \
	cd ../..

clean:
	if [ -f *.o ] ; then rm *.o ; fi
	if [ -f build/uxmux ] ; then rm build/uxmux ; fi
	if [ -f build/uxmux.map ] ; then rm build/uxmux.map ; fi

clean_all:
	if [ -f *.o ] ; then rm *.o ; fi
	if [ -f extra.elf ] ; then rm extra.elf ; fi
	if [ -f build/uxmux ] ; then rm build/uxmux ; fi
	if [ -f build/uxmux.map ] ; then rm build/uxmux.map ; fi
	cd lib/litehtml && \
	(make clean || :) && \
	if [ -f "CMakeCache.txt" ] ; then rm CMakeCache.txt ; fi && \
	if [ -f "cmake_install.cmake" ] ; then rm cmake_install.cmake ; fi && \
	if [ -d "CMakeFiles/" ] ; then rm -r CMakeFiles/ ; fi && \
	cd ../..



BASE_DIR = /root/Desktop
ZLIB_DIR = ${BASE_DIR}/zlib/final
PNG_DIR = ${BASE_DIR}/png/final
FREETYPE_DIR = ${BASE_DIR}/freetype/final
TARGETMACH = arm-linux-gnueabihf
BUILDMACH = ${MACHTYPE}
CROSS = arm-linux-gnueabihf
CC = ${CROSS}-gcc
LD = ${CROSS}-ld
AS = ${CROSS}-as
CXX = ${CROSS}-g++

cross_compile:
	if [ -d "${ZLIB_DIR}" ] && [ -d "${PNG_DIR}" ] && [ -d "${FREETYPE_DIR}" ] ; then \
		if [ -d "lib/litehtml/final/" ]	; then : ; \
		else \
			cd lib/litehtml && \
			(make clean || :) && \
			if [ -f "CMakeCache.txt" ] ; then rm CMakeCache.txt ; fi && \
			if [ -f "cmake_install.cmake" ] ; then rm cmake_install.cmake ; fi && \
			if [ -d "CMakeFiles/" ] ; then rm -r CMakeFiles/ ; fi && \
			export TARGETMACH=${TARGETMACH} && \
			export BUILDMACH=${BUILDMACH} && \
			export CROSS=${CROSS} && \
			export CC=${CC} && \
			export LD=${LD} && \
			export AS=${AS} && \
			export CXX=${CXX} && \
			cmake . && \
			make && \
		    mkdir final/ && \
			cp -r src/ final/src && \
			cp -r include/ final/include && \
			cp liblitehtml.a final/ && \
			cd ../.. ; \
		fi ; \
		if [ -d "final/" ] ; then : ; \
		else \
		    mkdir final/ ; \
		fi ; \
		${CXX} -fno-rtti -fno-builtin -Os -ffreestanding -isystem ${ZLIB_DIR}/include -isystem ${PNG_DIR}/include/libpng16 -isystem ${FREETYPE_DIR}/include/freetype2 -isystem lib/litehtml/final/include -c main.cpp ; \
		${CXX} -o final/uxmux main.o -s -v -nodefaultlibs -lc -lm -ldl -lstdc++ -lgcc_s -Wl,-t,-static -Llib/litehtml/final -llitehtml -L${FREETYPE_DIR}/lib -lfreetype -L${PNG_DIR}/lib -lpng16 -L${ZLIB_DIR}/lib -lz ; \
		cp lib/litehtml/final/include/master.css final/ ; \
	else \
		echo "FAILURE: Did you set up the cross-compiling toolchain?" ; \
	fi

clean_cross:
	if [ -d final/ ] ; then rm -r final/ ; fi
	if [ -d lib/litehtml/final/ ] ; then rm -r lib/litehtml/final/ ; fi
