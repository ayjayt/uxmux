#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "test_container.h"

void draw_test();

/* Convert r,g,b arguments to the associated pixel color */
inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
	return static_cast<uint32_t>(r<<vinfo->red.offset | g<<vinfo->green.offset | b<<vinfo->blue.offset);
}

int main(int argc, char* argv[]) {
	if (argc!=2) {
		std::cout << "usage:" << argv[0] << " <filename>" << std::endl;
	} else {
		/* Read the given file and print its contents to the screen */
		std::cout << "Reading file " << argv[1] << " as HTML:" << std::endl << std::endl;

		std::ifstream t(argv[1]);
		std::stringstream buffer;
		buffer << t.rdbuf();
		std::cout << buffer.str() << std::endl;

		/* Print the contents of the framebuffer before editing */
		// std::ifstream t2("/dev/fb0");
		// buffer << t2.rdbuf();
		// std::cout << buffer.str();
		// std::cout << std::endl << "----------------------------------------------" << std::endl;

		/* Edit the framebuffer and draw to the screen */
		draw_test();

		/* Print the contents of the framebuffer after editing */
		// std::ifstream t3("/dev/fb0");
		// buffer << t3.rdbuf();
		// std::cout << buffer.str();
		// std::cout << std::endl << "----------------------------------------------" << std::endl;

		std::cout << std::endl;

		/* Start litehtml rendering
		   See: https://github.com/litehtml/litehtml/wiki/How-to-use-litehtml */
		test_container painter;
		litehtml::context context;

		/* TODO: Finish this after test_container is implemented */
		std::cout << "createFromString" << std::endl;
		litehtml::document::createFromString(buffer.str().c_str(), &painter, &context);
		std::cout << "load_master_stylesheet" << std::endl;
		context.load_master_stylesheet(_t("html,div,body {display:block;} head,style {display:none;}"));

		std::cout << "Completed" << std::endl;
	}

	return 0;
}

/* Sample to show that fb drawing works
   See: http://betteros.org/tut/graphics1.php */
void draw_test() {
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	int tty_fd = open("/dev/tty0", O_RDWR);

	int fb_fd = open("/dev/fb0", O_RDWR);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale = 0;
	vinfo.bits_per_pixel = 32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	long screensize = vinfo.yres_virtual* finfo.line_length;
	std::cout << "Screen size: " << screensize << std::endl;

	uint8_t* fbp = static_cast<uint8_t*>(mmap(0, screensize, PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, off_t(0)));

	long x, y;

	for (x = 0; x < vinfo.xres; x++)
		for (y = 0; y < vinfo.yres; y++) {
			long location = (x+vinfo.xoffset)*(vinfo.bits_per_pixel/8) + (y+vinfo.yoffset)*finfo.line_length;
			*(reinterpret_cast<uint32_t*>(fbp+location)) = pixel_color(0xFF, 0x0, 0xFF, &vinfo);
		}

	ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);


	struct timespec tim, tim2;
	tim.tv_sec = 2;
	tim.tv_nsec = 0;
	if (nanosleep(&tim, &tim2)<0)
		std::cout << "nanosleep failed!" << std::endl;

	ioctl(tty_fd, KDSETMODE, KD_TEXT);
}
