#ifndef UXMUX_H
#define UXMUX_H

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
#include "uxmux.h"

/* TODO: After it works well, lets clean things up, make it neat. Look for optimizations, deal with warnings.. */

/* BEWARE: It seems that these can change on reboot */
#define MOUSE_MOVE_FILE "/dev/input/event7"
#define MOUSE_CLICK_FILE "/dev/input/mouse0"

#define FRAMEBUFFER_FILE "/dev/fb0"
#define TTY_FILE "/dev/tty0"

unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret, unsigned char last_click);
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo);
bool update(litehtml::document::ptr doc, uxmux_container* painter, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked);
void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw);

/* Render and draw to the screen*/
void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw) {
		if (redraw) {
		    /* Clear the screen to white */
			painter->draw_rect(painter->get_back_buffer(), 0, 0, static_cast<int>(vinfo->xres), static_cast<int>(vinfo->yres), 0xff, 0xff, 0xff);

			// printf("rendering..\n");
			doc->render(static_cast<int>(vinfo->xres));
			// printf("drawing..\n");
			// printf("   xscroll=%d, yscroll=%d\n", painter->get_x_scroll(), painter->get_y_scroll());
			litehtml::position clip({0,0,static_cast<int>(vinfo->xres),static_cast<int>(vinfo->yres)});
			doc->draw(painter->get_back_buffer(),painter->get_x_scroll(),painter->get_y_scroll(),&clip);
		}

		/* Render mouse on separate layer */
		painter->swap_buffer(painter->get_back_buffer(), hdcMouse, vinfo, finfo);
		painter->draw_scrollbars(hdcMouse);
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
bool update(litehtml::document::ptr doc, uxmux_container* painter, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked) {
	if (click_ie&0x10) return true;

	litehtml::position::vector redraw_box;
	*redraw = painter->update_scrollbars(doc, x, y, click_ie);
	*redraw |= doc->on_mouse_over(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	if (click_ie&0x1) {
		*is_clicked=true;
		*redraw |= doc->on_lbutton_down(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	} else if (*is_clicked) {
		*is_clicked = false;
		*redraw |= doc->on_lbutton_up(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	}
	// *redraw |= doc->on_mouse_leave(redraw_box);
	return click_ie&0x2;
}

/* Get framebuffer as a drawable object */
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo) {
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	int fb_fd = open(FRAMEBUFFER_FILE, O_RDWR);
	if (!fb_fd) return 0;
	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.bits_per_pixel = 32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	*_finfo = finfo;
	*_vinfo = vinfo;

	/* screensize = vinfo.yres_virtual * finfo.line_length */
	return static_cast<uint8_t*>(mmap(0, vinfo.yres_virtual*finfo.line_length, PROT_READ|PROT_WRITE, MAP_SHARED, fb_fd, off_t(0)));
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

	unsigned char click_ie[3] {last_click,0,0};
	bool y_flag(false), x_flag(false);
	int x(*x_ret), y(*y_ret);

	#define TARGET_X 800.0f
	#define TARGET_Y 600.0f

	#define MX_MIN 184.0f
	#define MX_MAX 65391.0f
	#define MY_MIN 245.0f
	#define MY_MAX 65343.0f
	#define MX_SCL 82.0f
	#define MY_SCL 109.0f

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

	x = static_cast<float>(x-MX_MIN)/(MX_MAX-MX_MIN)*TARGET_X;
	if (x>11) *x_ret = x;

	y = static_cast<float>(y-MY_MIN)/(MY_MAX-MY_MIN)*TARGET_Y;
	if (y>7) *y_ret = y;

	return click_ie[0]&(~0x10);
}

#endif
