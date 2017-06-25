#include <fstream>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "litehtml.h"
#include "uxmux_container.h"

/* TODO: After it works well, lets clean things up, make it neat. Look for optimizations, deal with warnings.. */

/* BEWARE: It seems that these can change on reboot */
#define MOUSE_MOVE_FILE "/dev/input/event7"
#define MOUSE_CLICK_FILE "/dev/input/mouse0"

#define FRAMEBUFFER_FILE "/dev/fb0"
#define TTY_FILE "/dev/tty0"

void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw);
bool update(litehtml::document::ptr doc, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked);
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo);
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret, unsigned char last_click);

int main(int argc, char* argv[]) {
	if (argc!=3) {
		printf("usage: %s <html_file> <master_css>\n", argv[0]);
	} else {
		int mmf, mcf;
		if ((mmf = open(MOUSE_MOVE_FILE, O_RDONLY))==-1) {
			printf("ERROR opening MOUSE_MOVE_FILE\n");
			mmf = 0;
			mcf = 0;
		} else if ((mcf = open(MOUSE_CLICK_FILE, O_RDONLY))==-1) {
			printf("ERROR opening MOUSE_CLICK_FILE\n");
			mmf = 0;
			mcf = 0;
		}

		/* Read the given file and print its contents to the screen */
		// printf("Reading file %s as HTML:\n\n", argv[1]);

		std::string prefix = argv[1];
		if (std::count(prefix.begin(), prefix.end(), '/') > 0)
			prefix = prefix.substr(0, prefix.find_last_of('/'));
		else prefix = "";

		std::ifstream t(argv[1]);
		std::stringstream buffer;
		buffer << t.rdbuf();
		// printf("%s\n", buffer.str().c_str());

		/* Load the master.css file */
		// printf("Loading file %s as Master CSS\n\n", argv[2]);

		std::ifstream t2(argv[2]);
		std::stringstream buffer2;
		buffer2 << t2.rdbuf();

		/* Get references to the framebuffer to work with */
		struct fb_fix_screeninfo finfo;
		struct fb_var_screeninfo vinfo;
		litehtml::uint_ptr hdc = get_drawable(&finfo, &vinfo);
		/* Separate draw layer for mouse */
		litehtml::uint_ptr hdcMouse = mmap(0, vinfo.yres_virtual * finfo.line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0));

		uxmux_container* painter = new uxmux_container(prefix, &finfo, &vinfo);
		litehtml::context context;

		/* Start litehtml rendering
		   See: https://github.com/litehtml/litehtml/wiki/How-to-use-litehtml */
		// printf("load_master_stylesheet\n");
		context.load_master_stylesheet(buffer2.str().c_str());
		// printf("createFromString\n");
		litehtml::document::ptr doc;
		doc = litehtml::document::createFromString(buffer.str().c_str(), painter, &context);

		bool done = false, redraw = true, is_clicked = false;
		int x=-1, y=-1;
		unsigned char click_ie = 0;

		/* Dirty fix for carryover click bug */
		if (mcf&&mmf) {
			printf("Right click screen to start. (Then again to exit.)\n");
			unsigned char val = handle_mouse(mcf, mmf, &x, &y, 0);
			while (!val&0x7)val=handle_mouse(mcf, mmf, &x, &y, 0);
			while (val&0x2||val==0)val=handle_mouse(mcf, mmf, &x, &y, 0);
		} else {
			printf("Press enter to start. (Then again to exit.)\n");
			int c;
			while (1) {
				c = getchar();
				if (isspace(c) || c == EOF)
					break;
			}
			click_ie = 0x10;
		}

		///////////////////////////////////////////////////////////
		int tty_fd = open(TTY_FILE, O_RDWR);
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);

		/* Main program loop */
		while (!done) {
			render(hdc, doc, painter, &vinfo, &finfo, hdcMouse, x, y, click_ie, redraw);

			click_ie = handle_mouse(mcf, mmf, &x, &y, click_ie);
			if(painter->check_new_page()) {
				std::string page = painter->get_directory()+painter->get_new_page();
				// printf("new_page: %s\n", page.c_str());

				std::ifstream t3(page.c_str());
				if (t3) {
					std::stringstream buffer3;
					buffer3 << t3.rdbuf();
					// printf("%s\n", buffer3.str().c_str());

					// printf("createFromString: %s\n", page.c_str());
					if (std::count(page.begin(), page.end(), '/') > 0)
						page = page.substr(0, page.find_last_of('/'));
					else page = "";
					painter = new uxmux_container(page, &finfo, &vinfo);
					doc = litehtml::document::createFromString(buffer3.str().c_str(), painter, &context);
					continue;
				} else if (painter->check_new_page_alt()) {
					page = painter->get_directory()+painter->get_new_page_alt();
					// printf("alt_page: %s\n", page.c_str());

					std::ifstream t4(page.c_str());
					if (t4) {
						std::stringstream buffer4;
						buffer4 << t4.rdbuf();
						// printf("%s\n", buffer4.str().c_str());

						// printf("createFromString: %s\n", page.c_str());
						if (std::count(page.begin(), page.end(), '/') > 0)
							page = page.substr(0, page.find_last_of('/'));
						else page = "";
						painter = new uxmux_container(page, &finfo, &vinfo);
						doc = litehtml::document::createFromString(buffer4.str().c_str(), painter, &context);
						continue;
					}
				}
			}
			done = update(doc, click_ie, x, y, &redraw, &is_clicked);
		}

		ioctl(tty_fd, KDSETMODE, KD_TEXT);
		///////////////////////////////////////////////////////////
	}

	// printf("Completed.\n");
	return 0;
}

/* Render and draw to the screen*/
void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw) {
		if (redraw) {
			painter->clear_screen();
			// printf("rendering..\n");
			doc->render(vinfo->xres);
			// printf("drawing..\n");
			doc->draw(painter->get_back_buffer(),0,0,0);
		}

		/* Render mouse on separate layer */
		painter->swap_buffer(hdcMouse);
		painter->draw_mouse(hdcMouse, x, y, click_ie);

		/* Copy buffer layer to screen layer */
		painter->swap_buffer(hdcMouse, hdc, vinfo, finfo);

		/* Hold the rendered screen for 2 seconds*/
		// struct timespec tim, tim2;
		// tim.tv_sec = 2;
		// tim.tv_nsec = 0;
		// if (nanosleep(&tim, &tim2)<0)
		// 	printf("nanosleep failed!");

		/* Hold screen until enter key is pressed*/
		if (click_ie&0x10) {
			while (1) {
				int c = getchar();
				if (isspace(c) || c == EOF)
					break;
			}
		}
}

/* Returns true when ready to stop program */
bool update(litehtml::document::ptr doc, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked) {
	if (click_ie&0x10) return true;

	litehtml::position::vector redraw_box;
	*redraw = false;
	*redraw |= doc->on_mouse_over(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	if (click_ie&0x1) {
		*is_clicked=true;
		*redraw |= doc->on_lbutton_down(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	} else if (*is_clicked) {
		*is_clicked = false;
		*redraw |= doc->on_lbutton_up(x, y, /*client_x*/ x, /*client_y*/ y, redraw_box);
	}
	// *redraw |= doc->on_mouse_leave(redraw_box);
	return click_ie&0x2;
}

/* Get fb0 as a drawable object */
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo) {
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	int fb_fd = open(FRAMEBUFFER_FILE, O_RDWR);
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
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret, unsigned char last_click) {
	if (!mcf || !mmf) return 0x10;

	fd_set read_fds, write_fds, except_fds;
	struct timeval timeout;
	struct input_event move_ie;

	unsigned char click_ie[3];
	click_ie[0] = last_click;
	bool y_flag=false, x_flag=false;
	int x = *x_ret, y = *y_ret;

	#define TARGET_X 800.0
	#define TARGET_Y 600.0

	#define MX_MIN 184.0
	#define MX_MAX 65391.0
	#define MY_MIN 245.0
	#define MY_MAX 65343.0
	#define MX_SCL 82.0
	#define MY_SCL 109.0

	/* Initialize file descriptor sets and set timeout */
	#define fd_ready_select(fd, sec, usec) \
		FD_ZERO(&read_fds); \
		FD_ZERO(&write_fds); \
		FD_ZERO(&except_fds); \
		FD_SET(fd, &read_fds); \
		timeout.tv_sec = sec; \
		timeout.tv_usec = usec;

	/* Set timeout to 1.0 microseconds */
	fd_ready_select(mcf, 0, 1);

	/* Update mouse click */
	if (select(mcf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
		// time_t timev;
		// time(&timev);
		read(mcf, &click_ie, sizeof(click_ie));
		// printf("click %d at pos (%d,%d) at time %ld\n", click_ie[0], x, y, timev);
	} else {
		/* timeout or error */
		// printf("click none\n");
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

	if (x>MX_MAX)x=MX_MAX;
	if (x<MX_MIN)x=MX_MIN;
	if (y>MY_MAX)y=MY_MAX;
	if (y<MY_MIN)y=MY_MIN;

	x = static_cast<double>(x-MX_MIN)/(MX_MAX-MX_MIN)*TARGET_X;
	if (x>11) *x_ret = x;

	y = static_cast<double>(y-MY_MIN)/(MY_MAX-MY_MIN)*TARGET_Y;
	if (y>7) *y_ret = y;

	return click_ie[0]&(~0x10);
}
