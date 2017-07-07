#ifndef UXMUX_CONTAINER_H
#define UXMUX_CONTAINER_H

#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unordered_map>
#include <fstream>
#include <dlfcn.h>
#include <exception>

#include "litehtml.h"
#include "image_loader.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#define SYSTEM_FONT_DIR "/usr/share/fonts/truetype/dejavu/"

/* No scrollbars by default */
#define DEFAULT_SCROLLBAR_SIZE 0

/* Bit flags to determine mouse state */
#define MOUSE_LEFT_CLICK 0x1
#define MOUSE_RIGHT_CLICK 0x2
#define MOUSE_MIDDLE_CLICK 0x4
#define MOUSE_ANY_CLICK 0x7
#define MOUSE_INVALID 0x10

/* Bit flags to determine scroll state */
#define SCROLL_EXCLUSIVE 0x1
#define SCROLL_SHARED 0x2

/* A litehtml::document_container for rendering to a framebuffer (uint32_t*) */
class uxmux_container : public litehtml::document_container {
private:
	struct font_structure {
		FT_Face font; // The font
		bool valid; // Flag to show font is ready for use
	};
	typedef struct font_structure font_structure_t;

	struct function_structure {
		void* function; // Pointer to the external function
		bool dynamic; // Flag to show whether it is dynamic (i.e. should be removed if it returns 0)
		bool ret_string; // Flag to show whether an always running (not dynamic) function returns a string
		std::string id; // HTML tag id to write a returned string from the function
	};
	typedef struct function_structure function_structure_t;

	std::unordered_map<std::string, font_structure_t> m_fonts; // Loaded fonts
	std::unordered_map<std::string, function_structure_t> m_functions; // Running external functions

	image_loader m_image_loader; // For drawing images
	void* m_handle; // Elf handle for loading external functions

	/* Framebuffer info variables */
	struct fb_fix_screeninfo* m_finfo;
	struct fb_var_screeninfo* m_vinfo;

	font_structure_t m_default_font; // Default font for use when other fonts not found

	/* Strings for the new HTML page file, alternative HTML page file, local directory, and font directory */
	std::string m_new_page, m_new_page_alt, m_directory, m_font_directory;

	/* Freetype library variables */
	FT_Library m_library;
	FT_Face m_face;
	FT_GlyphSlot m_slot;

	uint32_t* m_back_buffer; // Screen buffer for double buffering
	int m_client_width, m_client_height; // Screen size information

	/* Scrollbar related info (x = horizontal bar, y = vertical bar)*/
	int x_scroll, y_scroll, x_scrollbar_size, y_scrollbar_size; // Scroll amount and scrollbar sizes
	bool x_scrollable, y_scrollable; // Is the page scrollable in these directions
	bool m_scroll_cursor; // Is the mouse hovering a scrollbar
	int x_scroll_clicked, y_scroll_clicked; // Did the mouse click a scrollbar
	int x_start_scroll_pos, y_start_scroll_pos, x_start_click_pos, y_start_click_pos; // Saved starting info to calculate scroll difference
	litehtml::position x_scroll_rect, y_scroll_rect; // The drawing rectangles for the scrollbar handles (the draggable part of the scrollbar)

	bool m_show_mouse; // Whether we display a mouse
	bool m_cursor; // Shows mouse cursor in hover mode when true

public:

	///////////////////////////////////////////////////////////

	/* All member functions defined within this class, as it saves file size when compiling
		Listed here are method declarations for reader convenience */

	// uxmux_container(std::string prefix, std::string font_directory, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo);
	// ~uxmux_container(void);

	// void swap_buffer(litehtml::uint_ptr src_hdc, litehtml::uint_ptr dest_hdc, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo);
	// void draw_mouse(litehtml::uint_ptr hdc, int xpos, int ypos, unsigned char click);
	// void draw_scrollbars(litehtml::uint_ptr hdc);
	// bool handle_functions(litehtml::document::ptr doc);
	// bool update_scrollbars(litehtml::document::ptr doc, int xpos, int ypos, unsigned char click);
	// void draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, unsigned char red, unsigned char green, unsigned char blue);
	// void draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, unsigned char red, unsigned char green, unsigned char blue);
	// bool load_font(font_structure_t* font_struct);

	// std::string get_new_page();
	// std::string get_new_page_alt();
	// bool check_new_page();
	// bool check_new_page_alt();
	// uint32_t* get_back_buffer();
	// std::string get_directory();
	// std::string get_font_directory();
	// int get_x_scroll();
	// int get_y_scroll();

	/* The following are from abstract class "litehtml::document_container" in "litehtml/src/html.h"
	   see also: https://github.com/litehtml/litehtml/wiki/document_container */

	// litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override;
	// void delete_font(litehtml::uint_ptr hFont) override;
	// int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) override;
	// void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
	// int pt_to_px(int pt) override;
	// int get_default_font_size() const override;
	// const litehtml::tchar_t* get_default_font_name() const override;
	// void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
	// void load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) override;
	// void get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) override;
	// void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) override;
	// void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;
	// void set_caption(const litehtml::tchar_t* caption) override;
	// void set_base_url(const litehtml::tchar_t* base_url) override;
	// void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override;
	// void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) override;
	// void set_cursor(const litehtml::tchar_t* cursor) override;
	// void transform_text(litehtml::tstring& text, litehtml::text_transform tt) override;
	// void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) override;
	// void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y) override;
	// void del_clip() override;
	// void get_client_rect(litehtml::position& client) const override;
	// std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override;
	// void get_media_features(litehtml::media_features& media) const override;
	// void get_language(litehtml::tstring& language,litehtml::tstring& culture) const override;

	///////////////////////////////////////////////////////////

	/* Constructor:
		use of initializer list prevents unnecessary calls to default constructors of member variables */
	uxmux_container(std::string prefix, std::string font_directory, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo) :
		m_handle(0),
		m_finfo(finfo), m_vinfo(vinfo),
		m_default_font({0, false}),
		m_new_page(""), m_new_page_alt(""),
		m_directory((strcmp(prefix.c_str(),""))?(prefix+"/"):""),
		m_font_directory(font_directory),
		m_face(0), m_slot(0),
		m_back_buffer(static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0)))),
		m_client_width(static_cast<int>(vinfo->xres)),
		m_client_height(static_cast<int>(vinfo->yres)),
		x_scroll(0), y_scroll(0),
		x_scrollbar_size(DEFAULT_SCROLLBAR_SIZE),
		y_scrollbar_size(DEFAULT_SCROLLBAR_SIZE),
		x_scrollable(true), y_scrollable(true),
		m_scroll_cursor(false),
		x_scroll_clicked(0), y_scroll_clicked(0),
		x_start_scroll_pos(0), y_start_scroll_pos(0),
		x_start_click_pos(0), y_start_click_pos(0),
		x_scroll_rect({2, static_cast<int>(vinfo->yres)-DEFAULT_SCROLLBAR_SIZE+2, static_cast<int>(vinfo->xres)-DEFAULT_SCROLLBAR_SIZE-2, DEFAULT_SCROLLBAR_SIZE-4}),
		y_scroll_rect{static_cast<int>(vinfo->xres)-DEFAULT_SCROLLBAR_SIZE+2, 2, DEFAULT_SCROLLBAR_SIZE-4, static_cast<int>(vinfo->yres)-DEFAULT_SCROLLBAR_SIZE-2},
		m_show_mouse(true), m_cursor(false)
	{
		// printf("ctor uxmux_container\n");

		/* Keep font directory separate from local directory, as font directory does not change:
			the idea being that the default font directory will be the "fonts/" folder at the entry point location
			and as the page updates causing the local directory to change, this initial font directory should be
			passed on and remain static */
		if (m_font_directory=="")
			/* Specify font directory as "fonts/" folder in local directory */
			m_font_directory = m_directory+"fonts/";

		/* Setup Font Library */
		FT_Init_FreeType(&m_library);

		/* Clear the screen to white */
		draw_rect(m_back_buffer, 0, 0, static_cast<int>(m_vinfo->xres), static_cast<int>(m_vinfo->yres), 0xff, 0xff, 0xff);
	}



	/* Destructor */
	~uxmux_container(void) {
		// printf("dtor ~uxmux_container\n");
		/* Clean up fonts if loaded */
		if (m_library) {
			if (!m_fonts.empty()) {
				/* Iterate over loaded fonts */
				std::unordered_map<std::string, font_structure_t>::iterator it;
				for (it=m_fonts.begin(); it!=m_fonts.end(); ++it) {
					// printf("   delete_font: %s\n", it->first.c_str());
					if (it->second.valid && it->second.font) {
						/* Clean up the font and mark as invalid */
						FT_Done_Face(it->second.font);
						it->second.valid = false;
						it->second.font = 0;
					}
				}
				/* Clear all loaded fonts */
				m_fonts.clear();
			}
			/* Finally, clean up the font library */
			FT_Done_FreeType(m_library);
		}

		/* Clean up loaded functions */
		if (!m_functions.empty()) {
			m_functions.clear();
		}
	}



	/* Swap screen buffers, for double buffering */
	void swap_buffer(litehtml::uint_ptr src_hdc, litehtml::uint_ptr dest_hdc, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo) {
		int i;
		for (i=0; i<static_cast<int>(vinfo->yres_virtual * finfo->line_length/4); i++) {
			(reinterpret_cast<uint32_t*>(dest_hdc))[i] = reinterpret_cast<uint32_t*>(src_hdc)[i];
		}
	}



	/* Draw the mouse to the screen */
	void draw_mouse(litehtml::uint_ptr hdc, int xpos, int ypos, unsigned char click) {
		// printf("draw_mouse, at (%d, %d)\n", xpos, ypos);
		if (m_show_mouse) {
			/* Draw the mouse, in a color determined by click flag information */
			if (m_cursor || m_scroll_cursor) {
				/* Draw a mouse in a plus shape (to indicate hovering over clicable object) */
				draw_rect(hdc, xpos-1, ypos-4, 3, 9, (click & MOUSE_MIDDLE_CLICK) ? 0xff : 0, (click & MOUSE_RIGHT_CLICK) ? 0xff : 0, (click & MOUSE_LEFT_CLICK) ? 0xff : 0);
				draw_rect(hdc, xpos-4, ypos-1, 9, 3, (click & MOUSE_MIDDLE_CLICK) ? 0xff : 0, (click & MOUSE_RIGHT_CLICK) ? 0xff : 0, (click & MOUSE_LEFT_CLICK) ? 0xff : 0);
			} else {
				/* Draw mouse as a small circle */
				draw_rect(hdc, xpos-1, ypos-2, 3, 5, (click & MOUSE_MIDDLE_CLICK) ? 0xff : 0, (click & MOUSE_RIGHT_CLICK) ? 0xff : 0, (click & MOUSE_LEFT_CLICK) ? 0xff : 0);
				draw_rect(hdc, xpos-2, ypos-1, 5, 3, (click & MOUSE_MIDDLE_CLICK) ? 0xff : 0, (click & MOUSE_RIGHT_CLICK) ? 0xff : 0, (click & MOUSE_LEFT_CLICK) ? 0xff : 0);
			}
		}
	}



	/* Draw the scrollbars to the screen */
	void draw_scrollbars(litehtml::uint_ptr hdc) {
		if (y_scrollable) {
			/* Draw vertical scrollbar background */
			draw_rect(hdc, static_cast<int>(m_vinfo->xres) - y_scrollbar_size, 0, y_scrollbar_size, static_cast<int>(m_vinfo->yres), 0xaa, 0xaa, 0xaa);
			/* Draw vertical scrollbar handle */
			draw_rect(hdc, y_scroll_rect.x, y_scroll_rect.y, y_scroll_rect.width, y_scroll_rect.height, 0xdd, 0xdd, 0xdd);
		}
		if (x_scrollable) {
			/* Draw horizontal scrollbar background */
			draw_rect(hdc, 0, static_cast<int>(m_vinfo->yres) - x_scrollbar_size, static_cast<int>(m_vinfo->xres), x_scrollbar_size, 0xaa, 0xaa, 0xaa);
			/* Draw horizontal scrollbar handle */
			draw_rect(hdc, x_scroll_rect.x, x_scroll_rect.y, x_scroll_rect.width, x_scroll_rect.height, 0xdd, 0xdd, 0xdd);
		}
	}



	/* Run loaded external functions and return a flag indicating that the screen needs redrawing */
	bool handle_functions(litehtml::document::ptr doc) {
		/* Return "no redraw needed" by default */
		bool edit = false;
		if (!m_functions.empty()) {
			/* Keep a list of functions needing removal */
			std::vector<std::string> remove_me;
			remove_me.reserve(m_functions.size());

			/* Iterate over loaded functions */
			std::unordered_map<std::string, function_structure_t>::iterator it;
			for (it=m_functions.begin(); it!=m_functions.end(); ++it) {

				if (it->second.dynamic) {
					/* Handle dynamic type functions */
					int(*target)() = reinterpret_cast<int(*)()>(it->second.function);
					if (!target()) // Remove function if it returns 0
						remove_me.push_back(it->first);

				} else if (it->second.ret_string) {
					/* Handle always-running, string-returning functions */
					void(*target)(char*, int) = reinterpret_cast<void(*)(char*, int)>(it->second.function);

					/* Pass a string buffer and size to the function */
					char str[255]{0};
					target(str, 255);

					/* Retrieve the element matching the indicated id */
					litehtml::element::ptr el = doc->root()->select_one("#"+it->second.id);

					/* Write text to the element and set screen redraw flag */
					if (el) el->set_text(str);
					edit = true;

				} else {
					/* Handle always-running functions */
					void(*target)() = reinterpret_cast<void(*)()>(it->second.function);
					target(); // Simply run the function
				}
			}

			/* Remove each function marked for removal */
			unsigned int i;
			for (i=0; i<remove_me.size(); i++)
				m_functions.erase(remove_me[i]);
			remove_me.clear();
		}

		/* Return whether the screen needs redrawing */
		return edit;
	}



	/* Macro function: is a point within the given rectangle */
	#define isinside(xpos, ypos, rect) \
		(xpos>rect.x && xpos<rect.x+static_cast<int>(rect.width) && \
			ypos>rect.y && ypos<rect.y+static_cast<int>(rect.height))

	/* Update scrollbar positions and return flag indicating whether the screen needs redrawing
		For reference:
		- A scroll direction is "active exclusive" if it was activated through it's scroll bar handle
			(Active exclusive prevents the other scroll direction from being activated)
		- A scroll direction is "active shared" if it was activated through touch-screen behavior
			(Active shared allows the other scroll direction to be active shared only) */
	bool update_scrollbars(litehtml::document::ptr doc, int xpos, int ypos, unsigned char click) {

		/* Return "no redraw needed" by default */
		bool ret = false;

		/* Consider updating vertical bar only when:
			page is vertical scrollable, the horizontal scroll is not active exclusive,
			and the mouse is not over the horizontal scroll handle
				or
			the vertical scroll is already active */
		if ((y_scrollable && !(x_scroll_clicked & SCROLL_EXCLUSIVE) && !isinside(xpos, ypos, x_scroll_rect)) || y_scroll_clicked) {
			/* Calculate the height of the vertical scroll handle based on how much the screen can scroll and how large the viewable window is */
			y_scroll_rect.height = static_cast<float>(static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0)) / static_cast<float>(doc->height()) * (static_cast<float>(m_vinfo->yres)-static_cast<float>(x_scrollable?x_scrollbar_size:0));

			/* Mark scrollbar hover flag when:
				mouse is over the vertical scroll handle and the vertical scroll is not active
					or
				the vertical scroll is active exclusive */
			if ((isinside(xpos, ypos, y_scroll_rect) && !y_scroll_clicked) || (y_scroll_clicked & SCROLL_EXCLUSIVE))
				m_scroll_cursor = true;
			else
				m_scroll_cursor = false;

			/* Start scroll calculation when:
				scrollbar hover flag is set (see above) and left mouse is clicked
					or
				the cursor is not hovering over any on-screen, clickable element and the left mouse is clicked */
			if ((m_scroll_cursor && (click & MOUSE_LEFT_CLICK)) || (!m_cursor && (click & MOUSE_LEFT_CLICK))) {
				/* If vertical scroll not already active */
				if (!y_scroll_clicked) {
					/* Mark vertical scroll as active exclusive when hover flag is set, otherwise as active shared */
					y_scroll_clicked = m_scroll_cursor ? SCROLL_EXCLUSIVE : SCROLL_SHARED;

					/* Save initial scroll state for calculating scroll offset later */
					y_start_click_pos = ypos;
					y_start_scroll_pos = m_scroll_cursor?y_scroll_rect.y:y_scroll;
				}

				/* If vertical scroll is active shared */
				if (y_scroll_clicked & SCROLL_SHARED) {
					/* Find the vertical scroll offset based on mouse position change */
					y_scroll = y_start_scroll_pos - y_start_click_pos + ypos;
					/* Calculate the vertical scrollbar handle position based off of the vertical scroll offset */
					y_scroll_rect.y = -static_cast<float>(y_scroll)*static_cast<float>(static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0))/static_cast<float>(doc->height());

					/* Keep the vertical scrollbar handle within its bounds */
					if (y_scroll_rect.y > (static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0)) - static_cast<int>(y_scroll_rect.height)) {
					    y_scroll_rect.y = (static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0) - static_cast<int>(y_scroll_rect.height));
						/* Adjust the veritcal scroll offset accordingly to keep within bounds */
						y_scroll = -(static_cast<float>(y_scroll_rect.y) / static_cast<float>(static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0))) * static_cast<float>(doc->height());
					}

					/* Keep the vertical scrollbar handle within its bounds */
					if (y_scroll_rect.y < 2) {
						y_scroll_rect.y = 2;
						/* Adjust the veritcal scroll offset accordingly to keep within bounds */
						y_scroll = -(static_cast<float>(y_scroll_rect.y-2) / static_cast<float>(static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0)-2)) * static_cast<float>(doc->height());
					}

				/* Else vertical scroll must be active exclusive */
				} else {
					/* Find the vertical scrollbar handle position based on mouse position change */
					y_scroll_rect.y = y_start_scroll_pos - y_start_click_pos + ypos;

					/* Keep the vertical scrollbar handle within its bounds */
					if (y_scroll_rect.y > (static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0)) - static_cast<int>(y_scroll_rect.height))
					    y_scroll_rect.y = (static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0) - static_cast<int>(y_scroll_rect.height));

					/* Keep the vertical scrollbar handle within its bounds */
					if (y_scroll_rect.y < 2)
						y_scroll_rect.y = 2;

					/* Calculate the vertical scroll offset based on the vertical scrollbar handle position */
					y_scroll = -(static_cast<float>(y_scroll_rect.y-2) / static_cast<float>(static_cast<int>(m_vinfo->yres)-(x_scrollable?x_scrollbar_size:0)-2)) * static_cast<float>(doc->height());
				}

				/* Set return value flag, to indicate screen needs redrawing */
				ret = true;

			} else
				/* Deactivate vertical scroll */
				y_scroll_clicked = 0;
		}

		/* Consider updating horizontal bar only when:
			page is horizontal scrollable and the vertical scroll is not active exclusive */
		if (x_scrollable && !(y_scroll_clicked & SCROLL_EXCLUSIVE)) {
			/* Calculate the width of the horizontal scroll handle based on how much the screen can scroll and how large the viewable window is */
			x_scroll_rect.width = static_cast<float>(static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0)) / static_cast<float>(doc->width()) * (static_cast<float>(m_vinfo->xres)-static_cast<float>(y_scrollable?y_scrollbar_size:0));

			/* Mark scrollbar hover flag when:
				mouse is within horizontal scroll handle and the horizontal scroll is not active
					or
				the horizontal scroll is active exclusive
					or
				the mouse is over the vertical scrollbar, the horizontal scroll is not active, and the left mouse is not clicked */
			if ((isinside(xpos, ypos, x_scroll_rect) && !x_scroll_clicked) || (x_scroll_clicked & SCROLL_EXCLUSIVE) || (isinside(xpos, ypos, y_scroll_rect) && !x_scroll_clicked && !(click & MOUSE_LEFT_CLICK)))
				m_scroll_cursor = true;
			else
				m_scroll_cursor = false;

			/* Start scroll calculation when:
				scrollbar hover flag is set (see above) and left mouse is clicked
					or
				the cursor is not hovering over any on-screen, clickable element and the left mouse is clicked */
			if ((m_scroll_cursor && (click & MOUSE_LEFT_CLICK)) || (!m_cursor && (click & MOUSE_LEFT_CLICK))) {
				/* If horizontal scroll not already active */
				if (!x_scroll_clicked) {
					/* Mark horizontal scroll as active exclusive when hover flag is set, otherwise as active shared */
					x_scroll_clicked = m_scroll_cursor? SCROLL_EXCLUSIVE : SCROLL_SHARED;

					/* Save initial scroll state for calculating scroll offset later */
					x_start_click_pos = xpos;
					x_start_scroll_pos = m_scroll_cursor?x_scroll_rect.x:x_scroll;
				}

				/* If horizontal scroll is active shared */
				if (x_scroll_clicked & SCROLL_SHARED) {
					/* Find the horizontal scroll offset based on mouse position change */
					x_scroll = x_start_scroll_pos - x_start_click_pos + xpos;
					/* Calculate the horizontal scrollbar handle position based off of the horizontal scroll offset */
					x_scroll_rect.x = -static_cast<float>(x_scroll)*static_cast<float>(static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0))/static_cast<float>(doc->width());

					/* Keep the horizontal scrollbar handle within its bounds */
					if (x_scroll_rect.x > (static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0)) - static_cast<int>(x_scroll_rect.width)) {
					    x_scroll_rect.x = (static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0) - static_cast<int>(x_scroll_rect.width));
						/* Adjust the horizontal scroll offset accordingly to keep within bounds */
						x_scroll = -(static_cast<float>(x_scroll_rect.x) / static_cast<float>(static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0))) * static_cast<float>(doc->width());
					}

					/* Keep the horizontal scrollbar handle within its bounds */
					if (x_scroll_rect.x < 2) {
						x_scroll_rect.x = 2;
						/* Adjust the horizontal scroll offset accordingly to keep within bounds */
						x_scroll = -(static_cast<float>(x_scroll_rect.x-2) / static_cast<float>(static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0)-2)) * static_cast<float>(doc->width());
					}

				/* Else horizontal scroll must be active exclusive */
				} else {
					/* Find the horizontal scrollbar handle position based on mouse position change */
					x_scroll_rect.x = x_start_scroll_pos - x_start_click_pos + xpos;

					/* Keep the horizontal scrollbar handle within its bounds */
					if (x_scroll_rect.x > (static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0)) - static_cast<int>(x_scroll_rect.width))
					    x_scroll_rect.x = (static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0) - static_cast<int>(x_scroll_rect.width));

					/* Keep the horizontal scrollbar handle within its bounds */
					if (x_scroll_rect.x < 2)
						x_scroll_rect.x = 2;

					/* Calculate the horizontal scroll offset based on the horizontal scrollbar handle position */
					x_scroll = -(static_cast<float>(x_scroll_rect.x-2) / static_cast<float>(static_cast<int>(m_vinfo->xres)-(y_scrollable?y_scrollbar_size:0)-2)) * static_cast<float>(doc->width());
				}

				/* Set return value flag, to indicate screen needs redrawing */
				return true;

			} else
				/* Deactivate horizontal scroll */
				x_scroll_clicked = 0;
		}

		/* Return flag to indicate if screen needs redrawing */
		return ret;
	}



	/* Draw a colored rectangle to screen given litehtml::position object */
	void draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, unsigned char red, unsigned char green, unsigned char blue) {
		draw_rect(hdc, rect.x, rect.y, rect.width, rect.height, red, blue, green);
	}

	/* Draw a colored rectangle to the screen given (x, y) position, width, and height */
	void draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, unsigned char red, unsigned char green, unsigned char blue) {
		// printf("   draw_rect, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", xpos, ypos, width, height, red, green, blue);

		/* For every point within the bounds of the rectangle */
		int x, y;
		for (x = xpos; x < xpos + width; x++) {
			for (y = ypos; y < ypos + height; y++) {

				if (x < 0 || y < 0 || x >= static_cast<int>(m_vinfo->xres) || y >= static_cast<int>(m_vinfo->yres))
					/* Skip points which are outside the screen bounds */
					continue;

				/* Convert the (x, y) point to a location in the frame buffer */
				long location = (x+static_cast<int>(m_vinfo->xoffset))*(static_cast<int>(m_vinfo->bits_per_pixel)/8) + (y+static_cast<int>(m_vinfo->yoffset))*static_cast<int>(m_finfo->line_length);

				/* Mark the located framebuffer pixel as the specified color */
				*(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location)) = static_cast<uint32_t>(red<<m_vinfo->red.offset | green<<m_vinfo->green.offset | blue<<m_vinfo->blue.offset);
			}
		}
	}



	/* Check that a font structure is valid and loads the associated font,
		then returns a flag indicating if operation was successful */
	bool load_font(font_structure_t* font_struct) {
		/* Get the font from the font structure*/
		m_face = font_struct->font;

		if (!font_struct->valid || !m_face) {
			/* If the font is not valid, mark it as so */
			font_struct->valid = false;

			/* Load the default font if it exists */
			if (m_default_font.valid)
				m_face = m_default_font.font;

			/* Otherwise mark NULL font */
			else {
				m_face = 0;
			}
		}

		/* If NULL font, return failure */
		if (!m_face)
			return false;

		/* Load the font glyph and return success */
		m_slot = m_face->glyph;
		return true;
	}


	/* Get the new page if there is one
		(m_new_page holds the new HTML file that needs opening) */
	std::string get_new_page() {
		// printf("get_new_page\n");
		if (m_new_page != "") {
			std::string ret(m_new_page.c_str());
			m_new_page = "";
			return ret;
		}
		return 0;
	}

	/* Get the alternative new page if there is one
		(m_new_page_alt holds an alternative name for the new HTML file that needs opening) */
	std::string get_new_page_alt() {
		// printf("get_new_page_alt\n");
		if (m_new_page_alt != "") {
			std::string ret(m_new_page_alt.c_str());
			m_new_page_alt = "";
			return ret;
		}
		return 0;
	}

	/* Self descriptive getters */
	bool check_new_page() { return m_new_page != ""; }
	bool check_new_page_alt() { return m_new_page_alt != ""; }
	uint32_t* get_back_buffer() { return m_back_buffer; }
	std::string get_directory() { return m_directory; }
	std::string get_font_directory() { return m_font_directory; }
	int get_x_scroll() { return x_scroll; }
	int get_y_scroll() { return y_scroll; }



	/* The following are from abstract class "litehtml::document_container" in "litehtml/src/html.h"
	   see also: https://github.com/litehtml/litehtml/wiki/document_container */



	litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) {
		// printf("create_font: %s, size=%d, weight=%d, style=%d, decoration=%d\n", faceName, size, weight, italic, decoration);

		if (faceName) {

			std::string str = faceName;
			std::stringstream ss(str);
			std::string name;
			while (ss >> name) {
				if (ss.peek() == ',' || ss.peek() == '.' || ss.peek() == ' ') {
					ss.ignore();
					break;
				}
			}
			name.erase(std::remove(name.begin(), name.end(), '"'), name.end());
			name.erase(std::remove(name.begin(), name.end(), ','), name.end());
			// printf("   Parsed Name : %s\n", name.c_str());

			std::string mod = "";
			std::string modalt = "-Regular";
			if (italic && weight>=600) {
				mod = "-BoldItalic";
				modalt = "-BoldOblique";
			} else if (weight>=600) {
				mod = "-Bold";
				modalt = "-Heavy";
			} else if (italic) {
				mod = "-Italic";
				modalt = "-Oblique";
			}

			bool isdefault = false;
			if (m_fonts.count(name+mod+std::to_string(decoration)+std::to_string(size))) {
				/* If font already exists, retrieve it */
				isdefault = load_font(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
				/* Note we never save fonts using modalt so no need to check when loading */
			}

			if (!isdefault) {
				if (FT_New_Face(m_library, (m_font_directory+name+mod+".ttf").c_str(), 0, &m_face))
				if (FT_New_Face(m_library, (m_font_directory+name+mod+".otf").c_str(), 0, &m_face)) {
					// printf("   Error loading: fonts/%s.ttf/.otf\n   Looking in system instead..\n", (name+mod).c_str());
					if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+mod+".ttf").c_str(), 0, &m_face))
					if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+mod+".otf").c_str(), 0, &m_face)) {
						// printf("   Not found. Trying alternative: fonts/%s.ttf/.otf\n", (name+modalt).c_str());
						if (FT_New_Face(m_library, (m_font_directory+name+modalt+".ttf").c_str(), 0, &m_face))
						if (FT_New_Face(m_library, (m_font_directory+name+modalt+".otf").c_str(), 0, &m_face)) {
							// printf("   Error loading: fonts/%s.ttf/.otf\n   Looking in system instead..\n", (name+modalt).c_str());
							if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+modalt+".ttf").c_str(), 0, &m_face))
							if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+modalt+".otf").c_str(), 0, &m_face)) {
								printf("   WARNING: %s.ttf/.otf (alt %s.ttf/.otf) could not be found.\n", (name+mod).c_str(), (name+modalt).c_str());
								/* Try loading default font */
								name = get_default_font_name();
								if (m_fonts.count(name+mod+std::to_string(decoration)+std::to_string(size))) {
									isdefault = load_font(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
								}

								if (!isdefault) {
									if (FT_New_Face(m_library, (m_font_directory+name+mod+".ttf").c_str(), 0, &m_face))
									if (FT_New_Face(m_library, (m_font_directory+name+mod+".otf").c_str(), 0, &m_face)) {
										// printf("   Error loading: fonts/%s.ttf/.otf\n   Looking in system instead..\n", (name+mod).c_str());
										if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+mod+".ttf").c_str(), 0, &m_face))
										if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+mod+".otf").c_str(), 0, &m_face)) {
											// printf("   Not found. Trying alternative: fonts/%s.ttf/.otf\n", (name+modalt).c_str());
											if (FT_New_Face(m_library, (m_font_directory+name+modalt+".ttf").c_str(), 0, &m_face))
											if (FT_New_Face(m_library, (m_font_directory+name+modalt+".otf").c_str(), 0, &m_face)) {
												// printf("   Error loading: fonts/%s.ttf/.otf\n   Looking in system instead..\n", (name+modalt).c_str());
												if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+modalt+".ttf").c_str(), 0, &m_face))
												if (FT_New_Face(m_library, (SYSTEM_FONT_DIR+name+modalt+".otf").c_str(), 0, &m_face)) {
													printf("      WARNING: default_font [%s.ttf/.otf (alt %s.ttf/.otf)] could not be found.\n", (name+mod).c_str(), (name+modalt).c_str());
													/* If all else fails, try to see if we've loaded a font before and use that,
														this will cause "Critical Fail" message if it fails */
													if (m_default_font.valid)
														load_font(&m_default_font);
													isdefault = true;
												}
											}
										}
									}
								}
							}
						}
					}
				}

				if (!isdefault && m_face) {
					FT_Set_Char_Size(m_face, 0, size*16, 300, 300);
					m_face->generic.data = reinterpret_cast<void*>(decoration);

					/* Save the first valid font to be used if other fonts are invalid*/
					if (!m_default_font.valid) {
						m_default_font.font = m_face;
						m_default_font.valid = true;
					}

					m_slot = m_face->glyph;
					m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)] = font_structure_t({m_face, true});
				}
			}

			if (!m_face) {
				/* Because we always fall back to the default font, an empty font means no hope */
				throw std::invalid_argument("CRITICAL FAIL: no fonts found");
			}

			/* set up matrix */
			FT_Matrix matrix;
			// float angle = 0;
			matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
			matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
			matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
			matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

			fm->height = m_face->size->metrics.height/64;
			fm->ascent = m_face->size->metrics.ascender/32 - abs(m_face->size->metrics.descender/64);
			fm->descent = m_face->size->metrics.descender/32;

			FT_Vector pen;
			pen.x = 0;
			pen.y = 0;

			FT_Set_Transform(m_face, &matrix, &pen);
			FT_Load_Char(m_face, 'x', FT_LOAD_RENDER);

			fm->x_height = static_cast<int>(m_slot->bitmap.rows);

			// printf("      height=%d, ascent=%d, descent=%d, x_height=%d\n", fm->height, fm->ascent, fm->descent, fm->x_height);

			return reinterpret_cast<litehtml::uint_ptr>(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
		}

		return 0;
	}

	void delete_font(litehtml::uint_ptr hFont) {
		// printf("delete_font\n");
	}

	int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) {
		// printf("text_width\n");

		if (!load_font(reinterpret_cast<font_structure_t*>(hFont)))
			return 0;

		/* set up matrix */
		FT_Matrix matrix;
		// float angle = 0;
		matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
		matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
		matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
		matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

		FT_Vector pen;
		pen.x = 0;
		pen.y = 0;

		int n;
		for (n = 0; n < static_cast<int>(strlen(text)); n++) {
			FT_Set_Transform(m_face, &matrix, &pen);
			/* load glyph image into the slot (erase previous one) */
			if (FT_Load_Char(m_face, static_cast<FT_ULong>(text[n]), FT_LOAD_RENDER))
				continue;  /* ignore errors */

			if (n == static_cast<int>(strlen(text))-1)
				return m_slot->bitmap_left+m_slot->advance.x/64;

			/* increment pen position */
			pen.x += m_slot->advance.x;
			pen.y += m_slot->advance.y;
		}

		return 0;
	}

	void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
		// printf("draw_text: %s, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", text, pos.x, pos.y, pos.width, pos.height, static_cast<int>(color.red), static_cast<int>(color.green), static_cast<int>(color.blue));

		if (!load_font(reinterpret_cast<font_structure_t*>(hFont)))
			return;

		// printf("   decoration: %d\n", m_face->generic.data);

		int xpos = pos.x;
		int ypos = pos.y;

		/* Draw boxes where text should be */
		// if (!strcmp(text, " ")) {
		//     draw_rect(hdc, xpos, ypos, pos.width, pos.height, 0xff, 0, 0);
		// } else
		//     draw_rect(hdc, xpos, ypos, pos.width, pos.height, 0xff, 0xff, 0);

		/* set up matrix */
		FT_Matrix matrix;
		// float angle = 0;
		matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
		matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
		matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
		matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

		FT_Vector pen;
		pen.x = xpos * 64;
		pen.y = ypos * 64;

		int n;
		for (n = 0; n < static_cast<int>(strlen(text)); n++) {
			FT_Set_Transform(m_face, &matrix, &pen);
			/* load glyph image into the slot (erase previous one) */
			if (FT_Load_Char(m_face, static_cast<FT_ULong>(text[n]), FT_LOAD_RENDER))
				continue;  /* ignore errors */

			// printf("  bitmap_left=%d,  bitmap_top=%d, bitmap.width=%d, bitmap.rows=%d\n", m_slot->bitmap_left, m_slot->bitmap_top, m_slot->bitmap.width, m_slot->bitmap.rows);

			/* now, draw to our target surface (convert position) */
			int i, j, p, q;
			int targety = ypos + pos.height - m_slot->bitmap_top + ypos + m_face->size->metrics.descender/64;
			// printf("line height: %d\n", m_face->size->metrics.height/64);
			for (i = m_slot->bitmap_left, p = 0; i < m_slot->bitmap_left+static_cast<int>(m_slot->bitmap.width); i++, p++) {
				for (j = targety, q = 0; j < targety+static_cast<int>(m_slot->bitmap.rows); j++, q++) {
					if (i < 0 || j < 0 || i >= static_cast<int>(m_vinfo->xres) || j >= static_cast<int>(m_vinfo->yres))
					   continue;
					long location = (i+static_cast<int>(m_vinfo->xoffset))*(static_cast<int>(m_vinfo->bits_per_pixel)/8) + (j+static_cast<int>(m_vinfo->yoffset))*static_cast<int>(m_finfo->line_length);
					uint32_t col = m_slot->bitmap.buffer[q * static_cast<int>(m_slot->bitmap.width) + p];
					/* Split pixel color into RGB components */
					unsigned int col_r = (col&0xff0000)>>16;
					unsigned int col_g = (col&0xff00)>>8;
					unsigned int col_b = col&0xff;
					/* Find max of color components */
					unsigned int col_max;
					col_max = col_b > col_r ? col_b : col_r;
					col_max = col_max > col_g ? col_max : col_g;
					/* Draw the text in grayscale (usually black) */
					if (col) {
						uint32_t target = *(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location));
						/* Blend the text color into the background */
						/* color = alpha * (src - dest) + dest */
						col_r = (static_cast<float>(col_max)/static_cast<float>(0xff))*static_cast<float>(color.red-static_cast<float>((target>>16)&0xff))+((target>>16)&0xff);
						col_g = (static_cast<float>(col_max)/static_cast<float>(0xff))*static_cast<float>(color.green-static_cast<float>((target>>8)&0xff))+((target>>8)&0xff);
						col_b = (static_cast<float>(col_max)/static_cast<float>(0xff))*static_cast<float>(color.blue-static_cast<float>(target&0xff))+(target&0xff);
						*(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location)) = (col_r<<16)|(col_g<<8)|(col_b);
					}
				}
			}

			/* increment pen position */
			pen.x += m_slot->advance.x;
			pen.y += m_slot->advance.y;
		}

		/* draw underline */
		if (reinterpret_cast<long>(m_face->generic.data)&litehtml::font_decoration_underline) {
			draw_rect(hdc, xpos, ypos+pos.height-1, pos.width, 1, color.red, color.green, color.blue);
		}

		/* draw strikethrough */
		if (reinterpret_cast<long>(m_face->generic.data)&litehtml::font_decoration_linethrough) {
			draw_rect(hdc, xpos, ypos+pos.height/2, pos.width, 1, color.red, color.green, color.blue);
		}

		/* draw overline */
		if (reinterpret_cast<long>(m_face->generic.data)&litehtml::font_decoration_overline) {
			draw_rect(hdc, xpos, ypos, pos.width, 1, color.red, color.green, color.blue);
		}
	}

	int pt_to_px(int pt) {
		// printf("pt_to_px: %d\n", pt);
		return pt;
	}

	int get_default_font_size() const {
		// printf("get_default_font_size\n");
		return 16;
	}

	const litehtml::tchar_t* get_default_font_name() const {
		// printf("get_default_font_name\n");
		return "DejaVuSans";
	}

	/* TODO: Other list styles */
	/* For Reference:
		enum list_style_type
			{
				list_style_type_none,
				list_style_type_circle,
				list_style_type_disc,
				list_style_type_square,
				list_style_type_armenian,
				list_style_type_cjk_ideographic,
				list_style_type_decimal,
				list_style_type_decimal_leading_zero,
				list_style_type_georgian,
				list_style_type_hebrew,
				list_style_type_hiragana,
				list_style_type_hiragana_iroha,
				list_style_type_katakana,
				list_style_type_katakana_iroha,
				list_style_type_lower_alpha,
				list_style_type_lower_greek,
				list_style_type_lower_latin,
				list_style_type_lower_roman,
				list_style_type_upper_alpha,
				list_style_type_upper_latin,
				list_style_type_upper_roman,
			};
	*/
	void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
		// printf("draw_list_marker %s at (%d, %d)\n", marker.image.c_str(), marker.pos.x, marker.pos.y);
		draw_rect(hdc, marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height, marker.color.red, marker.color.green, marker.color.blue);
	}

	void load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) {
		// printf("load_image: %s\n", src);
	}

	void get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) {
		// printf("get_image_size: %s\n", src);
		m_image_loader.image_size((m_directory+src).c_str(), &sz.width, &sz.height);
	}

	void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) {
		// printf("draw_background: %s at (%d, %d)\n", bg.image.c_str(), bg.position_x, bg.position_y);
		if (strcmp(bg.image.c_str(), ""))
			if (!m_image_loader.load_image((m_directory+bg.image).c_str()))
				if (!m_image_loader.copy_to_framebuffer(hdc, m_finfo, m_vinfo, bg.position_x, bg.position_y))
					return;
		draw_rect(hdc, bg.border_box.x, bg.border_box.y, bg.border_box.width, bg.border_box.height, bg.color.red, bg.color.green, bg.color.blue);
	}

	/* TODO: Other border styles */
	/* For Reference:
		enum border_style
			{
				border_style_none,
				border_style_hidden,
				border_style_dotted,
				border_style_dashed,
				border_style_solid,
				border_style_double,
				border_style_groove,
				border_style_ridge,
				border_style_inset,
				border_style_outset
			};
	*/
	void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
		// printf("draw_borders\n");
		draw_rect(hdc, draw_pos.x, draw_pos.y, draw_pos.width-borders.right.width, borders.top.width, borders.top.color.red, borders.top.color.green, borders.top.color.blue);
		draw_rect(hdc, draw_pos.x, draw_pos.y+draw_pos.height-borders.bottom.width, draw_pos.width-borders.right.width, borders.bottom.width, borders.bottom.color.red, borders.bottom.color.green, borders.bottom.color.blue);
		draw_rect(hdc, draw_pos.x, draw_pos.y, borders.left.width, draw_pos.height, borders.left.color.red, borders.left.color.green, borders.left.color.blue);
		draw_rect(hdc, draw_pos.x+draw_pos.width-borders.right.width, draw_pos.y, borders.right.width, draw_pos.height, borders.right.color.red, borders.right.color.green, borders.right.color.blue);
	}

	void set_caption(const litehtml::tchar_t* caption) {
		// printf("set_caption: %s\n", caption);
	}

	void set_base_url(const litehtml::tchar_t* base_url) {
		// printf("set_base_url\n");
	}

	void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) {
		// printf("link: rel=%s, type=%s, href=%s\n", el->get_attr("rel"), el->get_attr("type"), el->get_attr("href"));
	}

	void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) {
		// printf("on_anchor_click: %s\n", url);
		std::string filename = url;
		if (!strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "elf")) {
			if(m_handle && el->get_attr("type")) {
				if (!strcmp(el->get_attr("type"), "function/static")) {
					void(*target)() = reinterpret_cast<void(*)()>(dlsym(m_handle, el->get_attr("target")));
					if (!target) {
						printf("%s\n", dlerror());
					} else
						target();
				} else if (!strcmp(el->get_attr("type"), "function/dynamic")) {
					std::string target = el->get_attr("target")?el->get_attr("target"):"";
					if (!m_functions.count(target)) {
						void* function = dlsym(m_handle, target.c_str());
						if (function)
							m_functions[target] = function_structure_t({function, true, false, ""});
					}
				}
			}
		} else if (!strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "html")) {
			m_new_page = filename;
			m_new_page_alt = "";
		} else if (filename != "") {
			m_new_page = filename+".html";
			m_new_page_alt = filename+"/index.html";
		}
	}

	void set_cursor(const litehtml::tchar_t* cursor) {
		// printf("set_cursor: %s\n", cursor);
		if (strcmp(cursor, "auto"))
			m_cursor = true;
		else if (!m_scroll_cursor)
			m_cursor = false;
	}

	void transform_text(litehtml::tstring& text, litehtml::text_transform tt) {
		// printf("transform_text: %s\n", text.c_str());
	}

	void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) {
		// printf("import_css: base=%s, url=%s\n", baseurl.c_str(), url.c_str());

		std::ifstream t(m_directory+url);
		// printf("    %s, url=%s\n", m_directory.c_str(), url.c_str());
		std::stringstream buffer;
		buffer << t.rdbuf();

		// printf("\n%s\n\n", buffer.str().c_str());

		text = buffer.str();
	}

	void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y) {
		// printf("set_clip\n");
	}

	void del_clip() {
		// printf("del_clip\n");
	}

	void get_client_rect(litehtml::position& client) const {
		// printf("get_client_rect (%d, %d)\n", m_vinfo->xres, m_vinfo->yres);
		client.x = 0;
		client.y = 0;
		client.width = m_client_width - (y_scrollable?y_scrollbar_size:0);
		client.height = m_client_height - (x_scrollable?x_scrollbar_size:0);
	}

	std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) {
		// printf("create_element: %s\n", tag_name);
		std::shared_ptr<litehtml::html_tag> element = std::shared_ptr<litehtml::html_tag>(new litehtml::html_tag(doc));
		element->set_tagName(tag_name);
		if (!attributes.empty()) {
			std::map<litehtml::tstring,litehtml::tstring>::const_iterator it;
			for (it=attributes.begin(); it!=attributes.end(); ++it) {
				// printf("   set_attr: %s=%s\n", it->first.c_str(), it->second.c_str());
				element->set_attr(it->first.c_str(), it->second.c_str());
				if (!strcmp(tag_name, "meta")) {
					if (it->first=="client-width") {
						try {
							m_client_width = std::stoi(it->second);
						} catch (std::exception e) {
							m_client_width = static_cast<int>(m_vinfo->xres);
						}
					} else if (it->first=="client-height") {
						try {
							m_client_height = std::stoi(it->second);
						} catch (std::exception e) {
							m_client_height = static_cast<int>(m_vinfo->yres);
						}
					} else if (it->first=="vertical-scroll") {
						y_scrollable = (it->second=="yes" || it->second=="true");
					} else if (it->first=="horizontal-scroll") {
						x_scrollable = (it->second=="yes" || it->second=="true");
					} else if (it->first=="vertical-scrollbar-size") {
						try {
							y_scrollbar_size = std::stoi(it->second);
						} catch (std::exception e) {
							printf("%s: Unable to parse vertical-scrollbar-size=%s\n", e.what(), it->second.c_str());
							y_scrollbar_size = 10;
						}
						y_scroll_rect.x = static_cast<int>(m_vinfo->xres)-y_scrollbar_size+2;
						y_scroll_rect.width = y_scrollbar_size-4;
						y_scroll_rect.height = static_cast<int>(m_vinfo->yres)-x_scrollbar_size-2;
						x_scroll_rect.width = static_cast<int>(m_vinfo->xres)-y_scrollbar_size-2;
					} else if (it->first=="horizontal-scrollbar-size") {
						try {
							x_scrollbar_size = std::stoi(it->second);
						} catch (std::exception e) {
							printf("%s: Unable to parse horizontal-scrollbar-size=%s\n", e.what(), it->second.c_str());
							x_scrollbar_size = 10;
						}
						x_scroll_rect.y = static_cast<int>(m_vinfo->yres)-x_scrollbar_size+2;
						x_scroll_rect.width = static_cast<int>(m_vinfo->xres)-y_scrollbar_size-2;
						x_scroll_rect.height = x_scrollbar_size-4;
						y_scroll_rect.height = static_cast<int>(m_vinfo->yres)-x_scrollbar_size-2;
					}
				}
			}

			if (!strcmp(tag_name, "link")) {
				if (!m_handle && element->get_attr("type") && !strcmp(element->get_attr("type"), "binary/elf")){
					m_handle = dlopen((m_directory+element->get_attr("href")).c_str(), RTLD_LAZY);
					if (!m_handle) {
						printf("%s\n", dlerror());
					}
				}
			} else if (!strcmp(tag_name, "text") && element->get_attr("href")) {
				std::string filename = element->get_attr("href");
				if (!strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "elf")) {
					if(m_handle && element->get_attr("type") && !strcmp(element->get_attr("type"), "function/always_run")) {
						std::string target = element->get_attr("target");
						if (!m_functions.count(target)) {
							void* function = dlsym(m_handle, target.c_str());
							if (function) {
								m_functions[target] = function_structure_t({function, false, element->get_attr("rel") ? !strcmp(element->get_attr("rel"),"string") : false, element->get_attr("id")?element->get_attr("id"):""});
							}
						}
					}
				}
			}
		}
		return 0;
	}

	void get_media_features(litehtml::media_features& media) const {
		// printf("get_media_features\n");
		litehtml::position client;
		get_client_rect(client);
		media.type = litehtml::media_type_screen;
		media.width = client.width;
		media.height = client.height;
		media.device_width = static_cast<int>(m_vinfo->xres);
		media.device_height = static_cast<int>(m_vinfo->yres);
		media.color = 8;
		media.monochrome = 0;
		media.color_index = 256;
		media.resolution = 96;
	}

	void get_language(litehtml::tstring& language, litehtml::tstring& culture) const {
		// printf("get_language\n");
		language = _t("en");
		culture = _t("");
	}
};

#endif
