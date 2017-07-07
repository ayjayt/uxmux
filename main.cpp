#include "uxmux.h"

/* Disables the graphics mode switching when true
	which prevents screen freeze should an error occur */
#define UXMUX_SAFE_MODE true

int main(int argc, char* argv[]) {
	if (argc!=3) {
		printf("usage: %s <html_file> <master_css>\n", argv[0]);
	} else {

		/* Open the mouse files for reading mouse clicks and movement */
		int mmf, mcf;
		if ((mmf = open(MOUSE_MOVE_FILE, O_RDONLY))==-1) {
			printf("ERROR opening %s\n", MOUSE_MOVE_FILE);
			/* If one fails they both fail */
			mmf = 0;
			mcf = 0;
		} else if ((mcf = open(MOUSE_CLICK_FILE, O_RDONLY))==-1) {
			printf("ERROR opening %s\n", MOUSE_CLICK_FILE);
			/* If one fails they both fail */
			mmf = 0;
			mcf = 0;
		}

		/* Extract the directory for the HTML file path for later use */
		std::string prefix = argv[1];
		if (std::count(prefix.begin(), prefix.end(), '/') > 0)
			prefix = prefix.substr(0, prefix.find_last_of('/'));
		else prefix = "";

		/* Read the given HTML file */
		// printf("Reading file %s as HTML:\n\n", argv[1]);
		std::ifstream t(argv[1]);
		std::stringstream html_buffer;
		html_buffer << t.rdbuf();
		// printf("%s\n", html_buffer.str().c_str());

		/* Read the given CSS file */
		// printf("Loading file %s as Master CSS\n\n", argv[2]);
		std::ifstream t2(argv[2]);
		std::stringstream css_buffer;
		css_buffer << t2.rdbuf();

		/* Get references to the framebuffer in order to draw to the screen */
		struct fb_fix_screeninfo finfo;
		struct fb_var_screeninfo vinfo;
		litehtml::uint_ptr hdc = get_drawable(&finfo, &vinfo);
		if (!hdc) {
			printf("ERROR opening %s\n", FRAMEBUFFER_FILE);
			/* If this fails, no use continuing */
			return 0;
		}

		/* Create a separate draw layer for the mouse */
		litehtml::uint_ptr hdcMouse = mmap(0, vinfo.yres_virtual * finfo.line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0));

		/* Setup litehtml variables
		   See: https://github.com/litehtml/litehtml/wiki/How-to-use-litehtml */
		uxmux_container* painter = new uxmux_container(prefix, "", &finfo, &vinfo);
		litehtml::context context;
		// printf("load_master_stylesheet\n");
		context.load_master_stylesheet(css_buffer.str().c_str());
		// printf("createFromString\n");
		litehtml::document::ptr doc;
		doc = litehtml::document::createFromString(html_buffer.str().c_str(), painter, &context);

		/* Setup useful flags and variables */
		bool done = false, redraw = true, is_clicked = false;
		int x=-1, y=-1;
		unsigned char click_ie = 0;

		/* Dirty fix for carryover click bug
			(Without this the program may immediately read a right click and end the program) */
		if (mcf && mmf) { // If mouse loading was successful
			printf("Right click screen to start. (Then again to exit.)\n");

			unsigned char val = handle_mouse(mcf, mmf, &x, &y, 0);
			/* Read mouse until any click value is found */
			while (!(val & MOUSE_ANY_CLICK))
				val = handle_mouse(mcf, mmf, &x, &y, 0);

			/* Read mouse until right click or empty value is found */
			while ((val & MOUSE_RIGHT_CLICK) || !val)
				val = handle_mouse(mcf, mmf, &x, &y, 0);

		} else { // Else use enter key to control program
			printf("Press enter to start. (Then again to exit.)\n");
			int c;
			/* Wait until user presses the enter key */
			while (1) {
				c = getchar();
				if (isspace(c) || c == EOF)
					break;
			}
			/* Set flag to show the mouse files weren't loaded */
			click_ie = MOUSE_INVALID;
		}

		///////////////////////////////////////////////////////////
		#if !UXMUX_SAFE_MODE
			/* Turn on graphics tty mode */
			int tty_fd = open(TTY_FILE, O_RDWR);
			ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
		#endif

		/* Main program loop */
		while (!done) {
			/* Draw to the screen */
			render(hdc, doc, painter, &vinfo, &finfo, hdcMouse, x, y, click_ie, redraw);

			/* Read mouse info */
			click_ie = handle_mouse(mcf, mmf, &x, &y, click_ie);

			/* Check if new page was loaded */
			if (painter->check_new_page()) {

				/* Get the file for the new HTML page and load it */
				std::string page = painter->get_directory()+painter->get_new_page();
				// printf("new_page: %s\n", page.c_str());
				std::ifstream t3(page.c_str());

				if (t3) { // If page exists
					/* Read the HTML file */
					std::stringstream new_html_page_buffer;
					new_html_page_buffer << t3.rdbuf();
					// printf("%s\n", new_html_page_buffer.str().c_str());

					/* Keep track of the new file location prefix */
					if (std::count(page.begin(), page.end(), '/') > 0)
						page = page.substr(0, page.find_last_of('/'));
					else page = "";

					/* Load the page into litehtml */
					painter = new uxmux_container(page, painter->get_font_directory(), &finfo, &vinfo);
					// printf("createFromString: %s\n", page.c_str());
					doc = litehtml::document::createFromString(new_html_page_buffer.str().c_str(), painter, &context);
					continue;

				} else if (painter->check_new_page_alt()) { // Else try alternate page file
					/* Get the file for the new HTML page and load it */
					page = painter->get_directory()+painter->get_new_page_alt();
					// printf("alt_page: %s\n", page.c_str());
					std::ifstream t4(page.c_str());

					if (t4) { // If page exists
						/* Read the HTML file */
						std::stringstream new_html_page_alt_buffer;
						new_html_page_alt_buffer << t4.rdbuf();
						// printf("%s\n", new_html_page_alt_buffer.str().c_str());

						/* Keep track of the new file location prefix */
						if (std::count(page.begin(), page.end(), '/') > 0)
							page = page.substr(0, page.find_last_of('/'));
						else page = "";

						/* Load the page into litehtml */
						painter = new uxmux_container(page, painter->get_font_directory(), &finfo, &vinfo);
						// printf("createFromString: %s\n", page.c_str());
						doc = litehtml::document::createFromString(new_html_page_alt_buffer.str().c_str(), painter, &context);
						continue;
					}
				}
			}

			/* Update screen contents and check if program exit is triggered */
			done = update(doc, painter, click_ie, x, y, &redraw, &is_clicked);
		}

		#if !UXMUX_SAFE_MODE
			/* Turn on text tty mode before exiting */
			ioctl(tty_fd, KDSETMODE, KD_TEXT);
		#endif
		///////////////////////////////////////////////////////////
	}

	// printf("Completed.\n");
	return 0;
}
