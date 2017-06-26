#ifndef UXMUX_CONTAINER_H
#define UXMUX_CONTAINER_H

#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unordered_map>
#include <fstream>
#include <dlfcn.h>

#include "litehtml.h"
#include "image_loader.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class uxmux_container : public litehtml::document_container {
private:
	struct font_structure {
		FT_Face font;
		bool valid;
	};
	typedef struct font_structure font_structure_t;

	std::unordered_map<std::string, font_structure_t> m_fonts;

	image_loader m_image_loader;
	void* m_handle;

	struct fb_fix_screeninfo* m_finfo;
	struct fb_var_screeninfo* m_vinfo;
	font_structure_t m_default_font;
	bool m_cursor;
	std::string m_new_page, m_new_page_alt, m_directory;

	FT_Library m_library;
	FT_Face m_face;
	FT_GlyphSlot m_slot;

	uint32_t* m_back_buffer;

public:
	uxmux_container(std::string prefix, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo) :
		m_handle(0), m_finfo(finfo), m_vinfo(vinfo), m_default_font({0, false}), m_cursor(false), m_new_page(""), m_new_page_alt(""), m_directory((strcmp(prefix.c_str(),"")!=0)?(prefix+"/"):""),
		m_face(0), m_slot(0), m_back_buffer(static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0))))
	{
		// printf("ctor uxmux_container\n");

		/* Setup Font Library */
		FT_Init_FreeType(&m_library);

		/* Clear the screen to white */
		draw_rect(m_back_buffer, 0, 0, static_cast<int>(m_vinfo->xres), static_cast<int>(m_vinfo->yres), 0xff, 0xff, 0xff);
	}

	~uxmux_container(void) {
		// printf("dtor ~uxmux_container\n");
		if (m_library) {
			if (!m_fonts.empty()) {
				std::unordered_map<std::string, font_structure_t>::iterator it;
				for (it=m_fonts.begin(); it!=m_fonts.end(); ++it) {
					// printf("   delete_font: %s\n", it->first.c_str());
					if (it->second.valid && it->second.font) {
						FT_Done_Face(it->second.font);
						it->second.valid = false;
						it->second.font = 0;
					}
				}
				m_fonts.clear();
			}
			FT_Done_FreeType(m_library);
		}
	}

	void swap_buffer(litehtml::uint_ptr src_hdc, litehtml::uint_ptr dest_hdc, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo) {
		int i;
		for (i=0; i<static_cast<int>(vinfo->yres_virtual * finfo->line_length/4); i++) {
			(reinterpret_cast<uint32_t*>(dest_hdc))[i] = reinterpret_cast<uint32_t*>(src_hdc)[i];
		}
	}

	void draw_mouse(litehtml::uint_ptr hdc, int xpos, int ypos, unsigned char click) {
		// printf("draw_mouse, at (%d, %d)\n", xpos, ypos);
		if (m_cursor) {
			draw_rect(hdc, xpos-1, ypos-4, 3, 9, click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0);
			draw_rect(hdc, xpos-4, ypos-1, 9, 3, click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0);
		} else {
			draw_rect(hdc, xpos-1, ypos-2, 3, 5, click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0);
			draw_rect(hdc, xpos-2, ypos-1, 5, 3, click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0);
		}
	}

	void draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, unsigned char red, unsigned char green, unsigned char blue) {
		draw_rect(hdc, rect.x, rect.y, rect.width, rect.height, red, blue, green);
	}

	void draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, unsigned char red, unsigned char green, unsigned char blue) {
		// printf("   draw_rect, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", xpos, ypos, width, height, red, green, blue);

		int x, y;
		for (x = xpos; x < xpos + width; x++) {
			for (y = ypos; y < ypos + height; y++) {
				if (x < 0 || y < 0 || x >= static_cast<int>(m_vinfo->xres) || y >= static_cast<int>(m_vinfo->yres))
					continue;
				long location = (x+static_cast<int>(m_vinfo->xoffset))*(static_cast<int>(m_vinfo->bits_per_pixel)/8) + (y+static_cast<int>(m_vinfo->yoffset))*static_cast<int>(m_finfo->line_length);
				*(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location)) = static_cast<uint32_t>(red<<m_vinfo->red.offset | green<<m_vinfo->green.offset | blue<<m_vinfo->blue.offset);
			}
		}
	}

	void load_font(font_structure_t* font_struct) {
		m_face = font_struct->font;
		if (!font_struct->valid || !m_face) {
			font_struct->valid = false;
			if (m_default_font.valid)
				m_face = m_default_font.font;
			else {
				m_face = 0;
				return;
			}
		}
		if (!m_face)
			return;
		m_slot = m_face->glyph;
	}

	std::string get_new_page() {
		// printf("get_new_page\n");
		if (m_new_page != "") {
			std::string ret(m_new_page.c_str());
			m_new_page = "";
			return ret;
		}
		return 0;
	}

	std::string get_new_page_alt() {
		// printf("get_new_page_alt\n");
		if (m_new_page_alt != "") {
			std::string ret(m_new_page_alt.c_str());
			m_new_page_alt = "";
			return ret;
		}
		return 0;
	}

	bool check_new_page() { return m_new_page != ""; }
	bool check_new_page_alt() { return m_new_page_alt != ""; }
	uint32_t* get_back_buffer() { return m_back_buffer; }
	std::string get_directory() { return m_directory; }

	/* From abstract class "litehtml::document_container" in "litehtml/src/html.h"
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
			// printf("   Parsed Name : %s\n", name.cstr());

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
				load_font(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
				if (m_face) isdefault = true;
				/* Note we never save fonts using modalt so no need to check when loading */
			}

			if (!isdefault) {
				if (FT_New_Face(m_library, (m_directory+"fonts/"+name+mod+".ttf").c_str(), 0, &m_face)) {
					// printf("   Error loading: fonts/%s.tff\n   Looking in system instead..\n", (name+mod).c_str());
					if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+name+mod+".ttf").c_str(), 0, &m_face)) {
						// printf("   Not found. Trying alternative: fonts/%s.tff\n", (name+modalt).c_str());
						if (FT_New_Face(m_library, (m_directory+"fonts/"+name+modalt+".ttf").c_str(), 0, &m_face)) {
							// printf("   Error loading: fonts/%s.tff\n   Looking in system instead..\n", (name+modalt).c_str());
							if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+name+modalt+".ttf").c_str(), 0, &m_face)) {
								printf("   WARNING: %s.tff (alt %s.tff) could not be found.\n", (name+mod).c_str(), (name+modalt).c_str());
								/* Try loading default font */
								name = get_default_font_name();
								if (m_fonts.count(name+mod+std::to_string(decoration)+std::to_string(size))) {
									load_font(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
									if (m_face) isdefault = true;
								}

								if (!isdefault) {
									if (FT_New_Face(m_library, (m_directory+"fonts/"+name+mod+".ttf").c_str(), 0, &m_face)) {
										// printf("   Error loading: fonts/%s.tff\n   Looking in system instead..\n", (name+mod).c_str());
										if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+name+mod+".ttf").c_str(), 0, &m_face)) {
											// printf("   Not found. Trying alternative: fonts/%s.tff\n", (name+modalt).c_str());
											if (FT_New_Face(m_library, (m_directory+"fonts/"+name+modalt+".ttf").c_str(), 0, &m_face)) {
												// printf("   Error loading: fonts/%s.tff\n   Looking in system instead..\n", (name+modalt).c_str());
												if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+name+modalt+".ttf").c_str(), 0, &m_face)) {
													printf("      WARNING: default_font [%s.tff (alt %s.tff)] could not be found.\n", (name+mod).c_str(), (name+modalt).c_str());
													/* If all else fails, try to see if we've loaded a font before and use that */
													if (!m_default_font.valid)
														return 0;
													/* Last option, will cause "Critical Fail" message (see below) if fails */
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
				printf("CRITICAL FAIL: while loading fonts\n");
				return 0;
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

		load_font(reinterpret_cast<font_structure_t*>(hFont));
		if (!m_face) return 0;

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

		load_font(reinterpret_cast<font_structure_t*>(hFont));
		if (!m_face) return;

		// printf("   decoration: %d\n", m_face->generic.data);

		int xpos = pos.x;
		int ypos = pos.y;

		/* Draw boxes where text should be */
		// if (strcmp(text, " ")==0) {
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
		if (strcmp(bg.image.c_str(), "")!=0)
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
		printf("link: rel=%s, type=%s, href=%s\n", el->get_attr("rel"), el->get_attr("type"), el->get_attr("href"));
		if (!m_handle && strcmp(el->get_attr("type"), "binary/elf")==0){
			m_handle = dlopen((m_directory+el->get_attr("href")).c_str(), RTLD_LAZY);
			if (!m_handle) {
				printf("%s\n", dlerror());
			}
		}
	}

	void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) {
		// printf("on_anchor_click: %s\n", url);
		std::string filename = url;
		if (strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "elf") == 0) {
			if(m_handle) {
				void(*target)() = reinterpret_cast<void(*)()>(dlsym(m_handle, el->get_attr("target")));
				if (!target) {
					printf("%s\n", dlerror());
				} else
					target();
			}
		} else if (strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "html") == 0) {
			m_new_page = filename;
			m_new_page_alt = "";
		} else if (filename != "") {
			m_new_page = filename+".html";
			m_new_page_alt = filename+"/index.html";
		}
	}

	void set_cursor(const litehtml::tchar_t* cursor) {
		// printf("set_cursor: %s\n", cursor);
		if (strcmp(cursor, "auto") != 0)
			m_cursor = true;
		else
			m_cursor = false;
	}

	void transform_text(litehtml::tstring& text, litehtml::text_transform tt) {
		printf("transform_text: %s\n", text.c_str());
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
		client.width = static_cast<int>(m_vinfo->xres);
		client.height = static_cast<int>(m_vinfo->yres);
	}

	std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) {
		// printf("create_element: %s\n", tag_name);
		litehtml::element element(doc);
		element.set_tagName(tag_name);
		if (!attributes.empty()) {
			std::map<litehtml::tstring,litehtml::tstring>::const_iterator it;
			for (it=attributes.begin(); it!=attributes.end(); ++it) {
				// printf("   set_attr: %s=%s\n", it->first.c_str(), it->second.c_str());
				element.set_attr(it->first.c_str(), it->second.c_str());
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
