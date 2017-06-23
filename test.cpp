#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input.h>
#include <sys/ioctl.h>

#include "test_container.h"

#define MOUSE_MOVE_FILE "/dev/input/event7"
#define MOUSE_CLICK_FILE "/dev/input/mouse0"

void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, test_container painter, struct fb_var_screeninfo vinfo);
bool update(litehtml::document::ptr doc);
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo);

int main(int argc, char* argv[]) {
	if (argc!=3) {
		std::cout << "usage: " << argv[0] << " <html_file> <master_css>" << std::endl;
	} else {

	    struct input_event move_ie;
	    unsigned char click_ie;

		int mmf, mcf;
		if ((mmf = open(MOUSE_MOVE_FILE, O_RDONLY))==-1) {
			std::cout << "ERROR opening MOUSE_MOVE_FILE" << std::endl;
			mmf = 0;
		} else if ((mcf = open(MOUSE_CLICK_FILE, O_RDONLY))==-1) {
			std::cout << "ERROR opening MOUSE_CLICK_FILE" << std::endl;
			mmf = 0;
			mcf = 0;
		}

		// /* Read the given file and print its contents to the screen */
		// std::cout << "Reading file " << argv[1] << " as HTML:" << std::endl << std::endl;

		// std::string path = argv[1];
		// std::string prefix = "";
		// if (std::count(path.begin(), path.end(), '/') > 0)
		// 	prefix = path.substr(0, path.find_last_of('/'));

		// std::ifstream t(argv[1]);
		// std::stringstream buffer;
		// buffer << t.rdbuf();
		// std::cout << buffer.str() << std::endl;

		// /* Load the master.css file */
		// std::cout << "Loading file " << argv[2] << " as CSS" << std::endl << std::endl;

		// std::ifstream t2(argv[2]);
		// std::stringstream buffer2;
		// buffer2 << t2.rdbuf();

		// /* Get references to the framebuffer to work with */
		// struct fb_fix_screeninfo finfo;
		// struct fb_var_screeninfo vinfo;
		// litehtml::uint_ptr hdc = get_drawable(&finfo, &vinfo);

		// test_container painter(prefix, &finfo, &vinfo);
		// litehtml::context context;

		// /* Start litehtml rendering
		//    See: https://github.com/litehtml/litehtml/wiki/How-to-use-litehtml */
		// std::cout << "load_master_stylesheet" << std::endl;
		// context.load_master_stylesheet(buffer2.str().c_str());
		// std::cout << "createFromString" << std::endl;
		// litehtml::document::ptr doc;
		// doc = litehtml::document::createFromString(buffer.str().c_str(), &painter, &context);

		bool done = false;

		/*
		mcf: 1 byte:
            left = data[0] & 0x1;
            right = data[0] & 0x2;
            middle = data[0] & 0x4;

		 mmf: type=3, time->time
			code=0 -> x
			code=1 -> y
			value -> val
		*/
		while (!done) {
			read(mcf, &click_ie, sizeof(unsigned char));
			printf("click %d\n", click_ie);
			if (click_ie&0x7) {
				read(mmf, &move_ie, sizeof(struct input_event));
				printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n", move_ie.time.tv_sec, move_ie.time.tv_usec, move_ie.type, move_ie.code, move_ie.value);
			}
			// render(hdc, doc, painter, vinfo);
			// done = update(doc);
		}
		std::cout << "Completed." << std::endl;
	}

	return 0;
}

/* Render and draw to the screen*/
void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, test_container painter, struct fb_var_screeninfo vinfo) {
		std::cout << "rendering.." << std::endl;
		doc->render(vinfo.xres);
		std::cout << "drawing.." << std::endl;
		doc->draw(painter.m_back_buffer,0,0,0);

		painter.swap_buffer(hdc);

		int tty_fd = open("/dev/tty0", O_RDWR);
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);

		/* Hold the rendered screen for 2 seconds*/
		// struct timespec tim, tim2;
		// tim.tv_sec = 2;
		// tim.tv_nsec = 0;
		// if (nanosleep(&tim, &tim2)<0)
		// 	std::cout << "nanosleep failed!" << std::endl;

		/* Hold screen until enter key is pressed*/
		while (std::cin.get() != '\n');

		ioctl(tty_fd, KDSETMODE, KD_TEXT);
}

bool update(litehtml::document::ptr doc) {
// 	bool							on_mouse_over(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
// 	bool							on_lbutton_down(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
// 	bool							on_lbutton_up(int x, int y, int client_x, int client_y, position::vector& redraw_boxes);
// 	bool							on_mouse_leave(position::vector& redraw_boxes);
	return true;
}

/* Get fb0 as a drawable object */
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo) {
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	int fb_fd = open("/dev/fb0", O_RDWR);
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale = 0;
	vinfo.bits_per_pixel = 32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	long screensize = vinfo.yres_virtual* finfo.line_length;
	uint8_t* fbp = static_cast<uint8_t*>(mmap(0, screensize, PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, off_t(0)));

	*_finfo = finfo;
	*_vinfo = vinfo;

	return fbp;
}
