#include "uxmux.h"

/* TODO: After it works well, lets clean things up, make it neat. Look for optimizations, deal with warnings.. */

/* Uncomment to easily disable the graphics mode switching*/
// #define UXMUX_SAFE_MODE 1

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
		#ifndef UXMUX_SAFE_MODE
		int tty_fd = open(TTY_FILE, O_RDWR);
		ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
		#endif

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

		#ifndef UXMUX_SAFE_MODE
		ioctl(tty_fd, KDSETMODE, KD_TEXT);
		#endif
		///////////////////////////////////////////////////////////
	}

	// printf("Completed.\n");
	return 0;
}
