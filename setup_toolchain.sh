#!/bin/bash

export BASE_DIR=/root/Desktop

echo ""
echo "Building Toolchain in ${BASE_DIR}"
echo ""

export ZLIB_SRC=${BASE_DIR}/zlib/src

export PNG_SRC=${BASE_DIR}/png/src
export PNG_BUILD=${BASE_DIR}/png/build

export FREETYPE_SRC=${BASE_DIR}/freetype/src
export FREETYPE_BUILD=${BASE_DIR}/freetype/build

export TARGETMACH=arm-linux-gnueabihf
export BUILDMACH=${MACHTYPE}
export CROSS=arm-linux-gnueabihf
export CC=${CROSS}-gcc
export LD=${CROSS}-ld
export AS=${CROSS}-as
export CXX=${CROSS}-g++


#####################################################
#                 Setup Z Library                   #
#####################################################

echo "---------------------------------"
echo ""
echo "Building Z Library"
echo ""

if [ -d "${BASE_DIR}/zlib" ]
then
    if [ -d "$ZLIB_SRC" ]
    then
    :
    else
        mkdir $FREETYPE_SRC
    fi
else
    mkdir -pv ${BASE_DIR}/zlib
    mkdir $ZLIB_SRC
fi

cd $ZLIB_SRC

if [ -f "zlib-1.2.11.tar.gz" ]
then
:
else
    wget zlib.net/zlib-1.2.11.tar.gz
    tar -pxzf zlib-1.2.11.tar.gz
fi

cd zlib-1.2.11/

./configure --prefix=${BASE_DIR}/zlib/final
sed -i 's/-O3/-Os/g' Makefile
make
make install


#####################################################
#                Setup PNG Library                  #
#####################################################

echo "---------------------------------"
echo ""
echo "Building PNG Library"
echo ""

if [ -d "${BASE_DIR}/png" ]
then
    if [ -d "$PNG_SRC" ]
    then
    :
    else
        mkdir $PNG_SRC
    fi
    if [ -d "$PNG_BUILD" ]
    then
    :
    else
        mkdir $PNG_BUILD
    fi
else
    mkdir -pv ${BASE_DIR}/png
    mkdir $PNG_SRC && mkdir $PNG_BUILD
fi

cd $PNG_SRC

if [ -f "libpng-1.6.8.tar.gz" ]
then
:
else
    wget http://sourceforge.net/projects/libpng/files/libpng16/older-releases/1.6.8/libpng-1.6.8.tar.gz
    tar -pxzf libpng-1.6.8.tar.gz
fi

cd ../build/
../src/libpng-1.6.8/./configure --prefix=${BASE_DIR}/png/final --host=$TARGETMACH CPPFLAGS=-I${BASE_DIR}/zlib/final/include LDFLAGS=-L${BASE_DIR}/zlib/final/lib
sed -i 's/-O2/-Os/g' Makefile
make
make install


#####################################################
#              Setup Freetype Library               #
#####################################################

echo "---------------------------------"
echo ""
echo "Building Freetype Library"
echo ""

if [ -d "${BASE_DIR}/freetype" ]
then
    if [ -d "$FREETYPE_SRC" ]
    then
    :
    else
        mkdir $FREETYPE_SRC
    fi
    if [ -d "$FREETYPE_BUILD" ]
    then
    :
    else
        mkdir $FREETYPE_BUILD
    fi
else
    mkdir -pv ${BASE_DIR}/freetype
    mkdir $FREETYPE_SRC && mkdir $FREETYPE_BUILD
fi

cd $FREETYPE_SRC

if [ -f "freetype-2.5.2.tar.gz" ]
then
:
else
    wget http://download.savannah.gnu.org/releases/freetype/freetype-2.5.2.tar.gz
    tar -pxzf freetype-2.5.2.tar.gz
fi

cd ../build/
../src/freetype-2.5.2/./configure --prefix=${BASE_DIR}/freetype/final --host=$TARGETMACH --without-png CFLAGS="-Os"
make
make install

#####################################################

echo "---------------------------------"
echo ""
echo "Building Toolchain Complete"
echo ""
