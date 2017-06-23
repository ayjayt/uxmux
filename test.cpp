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
#include <sys/select.h>

#include "test_container.h"

#define MOUSE_MOVE_FILE "/dev/input/event7"
#define MOUSE_CLICK_FILE "/dev/input/mouse0"

void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, test_container painter, struct fb_var_screeninfo vinfo);
bool update(litehtml::document::ptr doc, unsigned char click_ie, int x, int y, bool* redraw);
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo);
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret);

int main(int argc, char* argv[]) {
	if (argc!=3) {
		std::cout << "usage: " << argv[0] << " <html_file> <master_css>" << std::endl;
	} else {
		int mmf, mcf;
		if ((mmf = open(MOUSE_MOVE_FILE, O_RDONLY))==-1) {
			std::cout << "ERROR opening MOUSE_MOVE_FILE" << std::endl;
			mmf = 0;
		} else if ((mcf = open(MOUSE_CLICK_FILE, O_RDONLY))==-1) {
			std::cout << "ERROR opening MOUSE_CLICK_FILE" << std::endl;
			mmf = 0;
			mcf = 0;
		}

		/* Read the given file and print its contents to the screen */
		std::cout << "Reading file " << argv[1] << " as HTML:" << std::endl << std::endl;

		std::string path = argv[1];
		std::string prefix = "";
		if (std::count(path.begin(), path.end(), '/') > 0)
			prefix = path.substr(0, path.find_last_of('/'));

		std::ifstream t(argv[1]);
		std::stringstream buffer;
		buffer << t.rdbuf();
		std::cout << buffer.str() << std::endl;

		/* Load the master.css file */
		std::cout << "Loading file " << argv[2] << " as Master CSS" << std::endl << std::endl;

		std::ifstream t2(argv[2]);
		std::stringstream buffer2;
		buffer2 << t2.rdbuf();

		/* Get references to the framebuffer to work with */
		struct fb_fix_screeninfo finfo;
		struct fb_var_screeninfo vinfo;
		litehtml::uint_ptr hdc = get_drawable(&finfo, &vinfo);

		test_container painter(prefix, &finfo, &vinfo);
		litehtml::context context;

		/* Start litehtml rendering
		   See: https://github.com/litehtml/litehtml/wiki/How-to-use-litehtml */
		std::cout << "load_master_stylesheet" << std::endl;
		context.load_master_stylesheet(buffer2.str().c_str());
		std::cout << "createFromString" << std::endl;
		litehtml::document::ptr doc;
		doc = litehtml::document::createFromString(buffer.str().c_str(), &painter, &context);

		bool done = false, redraw = true;
		int x=-1, y=-1;
		unsigned char click_ie;

		int tty_fd = open("/dev/tty0", O_RDWR);
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);

		/* Main program loop */
		while (!done) {
			if (redraw)
				render(hdc, doc, painter, vinfo);

			click_ie = handle_mouse(mcf, mmf, &x, &y);
			done = update(doc, click_ie, x, y, &redraw);
		}

		ioctl(tty_fd, KDSETMODE, KD_TEXT);
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

		/* Hold the rendered screen for 2 seconds*/
		// struct timespec tim, tim2;
		// tim.tv_sec = 2;
		// tim.tv_nsec = 0;
		// if (nanosleep(&tim, &tim2)<0)
		// 	std::cout << "nanosleep failed!" << std::endl;

		/* Hold screen until enter key is pressed*/
		// while (std::cin.get() != '\n');
}

/* Returns true when ready to stop program */
bool update(litehtml::document::ptr doc, unsigned char click_ie, int x, int y, bool* redraw) {
	litehtml::position::vector redraw_box;
	*redraw = false;
	*redraw |= doc->on_mouse_over(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	if (click_ie&0x1) *redraw |= doc->on_lbutton_down(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	// *redraw |= doc->on_lbutton_up(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	// *redraw |= doc->on_mouse_leave(redraw_box);
	return click_ie&0x2;
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

/* Mouse Reference:
	mcf (click): 1 byte:
		left = data[0] & 0x1;
		right = data[0] & 0x2;
		middle = data[0] & 0x4;

	mmf (move): type=3, time->time
		code=0 -> x
		code=1 -> y
		value -> val
*/
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret) {
	fd_set read_fds, write_fds, except_fds;
	struct timeval timeout;
	struct input_event move_ie;

	unsigned char click_ie = 0;
	bool y_flag=false, x_flag=false;
	int x = *x_ret, y = *y_ret;

	/* Initialize file descriptor sets and set timeout */
	#define fd_ready_select(fd, sec, usec) \
		FD_ZERO(&read_fds); \
		FD_ZERO(&write_fds); \
		FD_ZERO(&except_fds); \
		FD_SET(fd, &read_fds); \
		timeout.tv_sec = sec; \
		timeout.tv_usec = usec;

	/* Set timeout to 1.0 seconds */
	fd_ready_select(mcf, 1, 0);

	/* Update mouse click */
	if (select(mcf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
		time_t timev;
		time(&timev);
		read(mcf, &click_ie, sizeof(unsigned char));
		printf("click %d at pos (%d,%d) at time %ld\n", click_ie, x, y, timev);
	} else {
		/* timeout or error */
		// std::cout << "click none" << std::endl;
	}

	/* Set timeout to 1.0 microseconds */
	fd_ready_select(mmf, 0, 1);

	/* Update mouse movement */
	if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
		read(mmf, &move_ie, sizeof(struct input_event));
		while (move_ie.type==0)
			read(mmf, &move_ie, sizeof(struct input_event));
		// printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n", move_ie.time.tv_sec, move_ie.time.tv_usec, move_ie.type, move_ie.code, move_ie.value);
		if (move_ie.code) {
			y = move_ie.value;
			y_flag = true;
		} else {
			x = move_ie.value;
			x_flag = true;
		}
		/* Set timeout to 1.0 microseconds */
		fd_ready_select(mmf, 0, 1);
		while (!y_flag || !x_flag) {
			if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
				read(mmf, &move_ie, sizeof(struct input_event));
				while (move_ie.type==0) {
					/* Set timeout to 1.0 microseconds */
					fd_ready_select(mmf, 0, 1);
					if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1)
						read(mmf, &move_ie, sizeof(struct input_event));
					else break;
				}
				// printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n", move_ie.time.tv_sec, move_ie.time.tv_usec, move_ie.type, move_ie.code, move_ie.value);
				if (move_ie.type && move_ie.code && !y_flag) {
					y = move_ie.value;
					y_flag = true;
				} else if (move_ie.type && !move_ie.code && !x_flag) {
					x = move_ie.value;
					x_flag = true;
				}
				/* Set timeout to 1.0 microseconds */
				fd_ready_select(mmf, 0, 1);
			} else break;
		}
		/* Set timeout to 10.0 microseconds */
		fd_ready_select(mmf, 0, 10);
		while(1) {
			if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
				read(mmf, &move_ie, sizeof(struct input_event));
				/* Set timeout to 10.0 microseconds */
				fd_ready_select(mmf, 0, 10);
			} else break;
		}
	}

	*x_ret = x;
	*y_ret = y;
	return click_ie;
}
