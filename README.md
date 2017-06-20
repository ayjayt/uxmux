# liteHTML_fb:

### Keelin:

Tested on linux (4.9.0-kali4-amd64) <br/>
gcc version 6.3.0 20170516 (Debian 6.3.0-18) <br/>
cmake version 3.7.2 <br/>
[litehtml](https://github.com/litehtml/litehtml) version at d7cc6ab (Apr 7, 2017)

### My steps, assuming "." is project directory:
1. Download litehtml and place folder into "./lib/" directory
2. Compile litehtml from "./lib/litehtml/" using `cmake .` then `make`
3. Compile project from "." using `make`
4. Run the compiled program from "./build" using `./test <filename>`

I added a "--force-show" argument which changes the mode between KD_GRAPHICS and KD_TEXT so that the framebuffer graphics can actually be seen. It is sort of hacky and may be unique to my environment (a linux desktop environment). <br/>
Using this flag is likely to blackout your screen on a desktop environment. Although you can still interact with the underlying desktop, it cannot be seen. Should you get stuck in a full black screen (KD_TEXT mode), pressing Ctrl-Alt-F1 works for me to escape it. <br/>
See: http://betteros.org/tut/graphics1.php

TODO: implement test_container.cpp (a litehtml::document_container)

---
