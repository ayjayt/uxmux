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

/* BEWARE: It seems that these can change on reboot..
		also note, they might be the same file */
#define MOUSE_MOVE_FILE "/dev/input/event7"
#define MOUSE_CLICK_FILE "/dev/input/mouse0"

#define FRAMEBUFFER_FILE "/dev/fb0"
#define TTY_FILE "/dev/tty0"

void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw);
bool update(litehtml::document::ptr doc, uxmux_container* painter, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked);
litehtml::uint_ptr get_drawable(struct fb_fix_screeninfo *_finfo, struct fb_var_screeninfo *_vinfo);
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret, unsigned char last_click);

/* Render and draw to the screen*/
void render(litehtml::uint_ptr hdc, litehtml::document::ptr doc, uxmux_container* painter, struct fb_var_screeninfo* vinfo, struct fb_fix_screeninfo* finfo, litehtml::uint_ptr hdcMouse, int x, int y, unsigned char click_ie, bool redraw) {
		if (redraw) {
			/* Clear the screen to white */
			painter->draw_rect(painter->get_back_buffer(), 0, 0, static_cast<int>(vinfo->xres), static_cast<int>(vinfo->yres), 0xff, 0xff, 0xff);

			// printf("rendering..\n");
			doc->render(static_cast<int>(vinfo->xres));

			// printf("drawing..\n");
			// printf("   xscroll=%d, yscroll=%d\n", painter->get_x_scroll(), painter->get_y_scroll());

			/* Specify a clip to make sure we only compute objects in visible portion of screen */
			litehtml::position clip({0,0,static_cast<int>(vinfo->xres),static_cast<int>(vinfo->yres)});
			/* Draw to the back buffer, passing the screen scroll info and clip */
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
		//	printf("nanosleep failed!");

		/* If mouse is invalid (mouse files failed loading), hold screen until enter key is pressed */
		if (click_ie & MOUSE_INVALID) {
			while (1) {
				int c = getchar();
				if (isspace(c) || c == EOF)
					break;
			}
		}
}

/* Returns true when ready to stop program */
bool update(litehtml::document::ptr doc, uxmux_container* painter, unsigned char click_ie, int x, int y, bool* redraw, bool* is_clicked) {
	if (click_ie & MOUSE_INVALID) return true;

	litehtml::position::vector redraw_box;
	*redraw = painter->update_scrollbars(doc, x, y, click_ie);
	*redraw |= doc->on_mouse_over(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	if (click_ie & MOUSE_LEFT_CLICK) { // Handle mouse click
		*is_clicked=true;
		*redraw |= doc->on_lbutton_down(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	} else if (*is_clicked) { // Handle mouse release
		*is_clicked = false;
		*redraw |= doc->on_lbutton_up(x-painter->get_x_scroll(), y-painter->get_y_scroll(), /*client_x*/x-painter->get_x_scroll(), /*client_y*/y-painter->get_y_scroll(), redraw_box);
	}
	// *redraw |= doc->on_mouse_leave(redraw_box);

	*redraw |= painter->handle_functions(doc);

	return click_ie & MOUSE_RIGHT_CLICK;
}

/* Get framebuffer as a drawable object and get associated screeninfo variables */
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

	see: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/input-event-codes.h
	mmf (move): type={3 for MOUSE_ABS (absolute) | 2 for MOUSE_REL (relative)}, time->time
		code=0 -> x
		code=1 -> y
		value -> val
		if MOUSE_ABS: x, y treated as precise coordinates
			else they are treated as change in position
*/
/* Handles a mouse input
	TODO: This will be made obsolete as we change over to touch screen input, unless the final user would use a mouse */
unsigned char handle_mouse(int mcf, int mmf, int* x_ret, int* y_ret, unsigned char last_click) {
	if (!mcf || !mmf) return MOUSE_INVALID;

	fd_set read_fds, write_fds, except_fds;
	struct timeval timeout;
	struct input_event move_ie;

	unsigned char click_ie[3] {last_click,0,0};
	bool y_flag(false), x_flag(false);
	int x(*x_ret), y(*y_ret);

	/* The width and height of the target screen
		TODO: probably should have just used dimensions
			from uxmux_container's m_client_width/height */
	#define TARGET_X 800.0f
	#define TARGET_Y 600.0f

	/* Screen correction dimmensions, very unique to keelins VM screen..
		Only used in exact mouse format (MOUSE_ABS) */
	#define MX_MIN 184.0f
	#define MX_MAX 65391.0f
	#define MY_MIN 245.0f
	#define MY_MAX 65343.0f
	#define MX_SCL 82.0f
	#define MY_SCL 109.0f

	#define MOUSE_ABS 3
	#define MOUSE_REL 2

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

	bool y_exact = false, x_exact = false;

	/* Update mouse movement */
	/* TODO: The extra while loop pass through is not needed as instead of `while (!move_ie.type)` we can just do
			`while (move_ie.type!=MOUSE_ABS && move_ie.type!=MOUSE_REL)` from the start (probably in `do while` loop).
				Not making the change now as I don't have time to test, in case it breaks something */
	if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
		read(mmf, &move_ie, sizeof(struct input_event));

		/* Read file until we get non-zero type info */
		/* TODO: Are we always guaranteed a non-zero type somewhere in the file?
			or could this be safer in a select statement also? (i.e safe from while loop freeze) */
		while (!move_ie.type)
			read(mmf, &move_ie, sizeof(struct input_event));

		// printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n", move_ie.time.tv_sec, move_ie.time.tv_usec, move_ie.type, move_ie.code, move_ie.value);
		/* Record x or y value if valid type */
		if ((move_ie.type==MOUSE_ABS || move_ie.type==MOUSE_REL) && move_ie.code==1) {
			y = move_ie.value;
			y_flag = true;
			y_exact = (move_ie.type==MOUSE_ABS);
		} else if ((move_ie.type==MOUSE_ABS || move_ie.type==MOUSE_REL) && !move_ie.code) {
			x = move_ie.value;
			x_flag = true;
			x_exact = (move_ie.type==MOUSE_ABS);
		}
		/* Set timeout to 1.0 microseconds */
		fd_ready_select(mmf, 0, 1);
		/* Keep trying until a valid type is found (or until select timeout) */
		while (!y_flag || !x_flag) {
			if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
				read(mmf, &move_ie, sizeof(struct input_event));

				/* Filter out all non valid types */
				while (move_ie.type!=MOUSE_ABS && move_ie.type!=MOUSE_REL) {
					/* Set timeout to 1.0 microseconds */
					fd_ready_select(mmf, 0, 1);
					if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1)
						read(mmf, &move_ie, sizeof(struct input_event));
					else break;
				}
				// printf("time %ld.%06ld\ttype %d\tcode %d\tvalue %d\n", move_ie.time.tv_sec, move_ie.time.tv_usec, move_ie.type, move_ie.code, move_ie.value);
				/* Record x or y value if valid type */
				if (move_ie.type && move_ie.code==1 && !y_flag) {
					y = move_ie.value;
					y_flag = true;
					y_exact = (move_ie.type==MOUSE_ABS);
				} else if (move_ie.type && !move_ie.code && !x_flag) {
					x = move_ie.value;
					x_flag = true;
					x_exact = (move_ie.type==MOUSE_ABS);
				}
				/* Set timeout to 1.0 microseconds */
				fd_ready_select(mmf, 0, 1);
			} else break;
		}
		/* Set timeout to 10.0 microseconds */
		fd_ready_select(mmf, 0, 10);
		/* Clear out extra data in file
			TODO:
			WARNING: (Potential to freeze if mouse file gets data faster than this can dump,
				but this whole function isn't needed in final product anyways,
				as we will switch to touch input, so not worried now) */
		while(1) {
			if (select(mmf + 1, &read_fds, &write_fds, &except_fds, &timeout) == 1) {
				read(mmf, &move_ie, sizeof(struct input_event));
				/* Set timeout to 10.0 microseconds */
				fd_ready_select(mmf, 0, 10);
			} else break;
		}
	}

	/* Fix the y value in an appropriate range */
	if (y_exact && y_flag) {
		if (y>MY_MAX)y=MY_MAX;
		if (y<MY_MIN)y=MY_MIN;

		y = static_cast<float>(y-MY_MIN)/(MY_MAX-MY_MIN)*TARGET_Y;
		if (y>7) *y_ret = y;
	} else if (y_flag) {
		*y_ret += y;

		if (*y_ret>TARGET_Y)*y_ret=TARGET_Y;
		if (*y_ret<0)*y_ret=0;
	}

	/* Fix the x value in an appropriate range */
	if (x_exact && x_flag) {
		if (x>MX_MAX)x=MX_MAX;
		if (x<MX_MIN)x=MX_MIN;

		x = static_cast<float>(x-MX_MIN)/(MX_MAX-MX_MIN)*TARGET_X;
		if (x>11) *x_ret = x;
	} else if (x_flag) {
		*x_ret += x;

		if (*x_ret>TARGET_X)*x_ret=TARGET_X;
		if (*x_ret<0)*x_ret=0;
	}

	/* Return the click type while forcing the invalid flag low */
	return click_ie[0]&(~MOUSE_INVALID);
}

#endif
