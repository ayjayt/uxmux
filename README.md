# liteHTML_fb:

### Keelin:

Tested on linux (4.9.0-kali4-amd64) <br/>
gcc version 6.3.0 20170516 (Debian 6.3.0-18) <br/>
cmake version 3.7.2 <br/>
freetype2 version 18.3.12 <br/>
libpng version 1.6.28 <br/>
[litehtml](https://github.com/litehtml/litehtml) version at d7cc6ab (Apr 7, 2017)

### My steps, assuming "." is project directory:
1. Download litehtml and place folder into "./lib/" directory
2. Compile litehtml from "./lib/litehtml/" using `cmake .` then `make`
3. Compile project from "." using `make`
4. Run the compiled program from "./build" using `./uxmux <html_file> <master_css>`
(Right click the screen to exit the program.)

### Oliver:

I added 'toolchain_file' to be used by cmake for cross-compiling to arm.
You'll have to replace '/mnt/beagle' with your mountpoint.
The cmake command is: cmake -DCMAKE_TOOLCHAIN_FILE=toolchain_file .

For more info: http://www.vtk.org/Wiki/CMake_Cross_Compiling

---
