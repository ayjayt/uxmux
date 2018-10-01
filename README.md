# UXMux

### Notes:

Tested on linux (4.9.0-kali4-amd64)           <br/>
gcc version 6.3.0 20170516 (Debian 6.3.0-18)  <br/>
cmake version 3.7.2                           <br/>
freetype2 version 18.3.12                     <br/>
libpng version 1.6.28                         <br/>

### My steps, assuming "." is project directory:
1. Compile the liteHTML_fb project from "." using `make`
2. Compile external .elf (which can be loaded into application using HTML) from "." using `make extra`
3. Run the compiled program from "./build" using `./uxmux <html_file> <master_css>`

### Cross-compiling for BBB steps, assuming "." is project directory:

#### Toolchain setup:
1. Configure the installation directory for the toolchain by changing "BASE\_DIR" in "./setup_toolchain.sh" and "./makefile"
2. Make sure you have the arm-linux-gnueabihf toolchain and compilers installed (example: `apt-get install gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf`)
3. Download and build the rest of the toolchain by running `./setup_toolchain.sh`
 (you will need to run `chmod +x ./setup_toolchain.sh` to do so, as it is recommended that you understand the script
 or at least configure the installation directory before executing it)

#### Cross-compiling:
1. Cross-compile the liteHTML_fb project from "." using `make cross_compile`
2. Copy the compiled program from "./final" to the target device
