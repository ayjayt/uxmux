#ifndef UXMUX_CONTAINER_H
#define UXMUX_CONTAINER_H

#include <linux/fb.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unordered_map>
#include <fstream>

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
	font_structure_t m_default_font;

	std::unordered_map<std::string, font_structure_t> m_fonts;

	FT_Library m_library;
	FT_Face m_face;
	FT_GlyphSlot m_slot;

    image_loader m_image_loader;

	std::string m_directory, m_new_page, m_new_page_alt;
	struct fb_fix_screeninfo* m_finfo;
	struct fb_var_screeninfo* m_vinfo;

	bool m_cursor;
	uint32_t* m_back_buffer;

public:
	uxmux_container(std::string prefix, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo) :
	    m_finfo(finfo), m_vinfo(vinfo), m_default_font({0, false}), m_cursor(false), m_new_page(""), m_new_page_alt("")
	{
	    // printf("ctor uxmux_container\n");

	    if (strcmp(prefix.c_str(),"")!=0)
	        m_directory = prefix+"/";
	    else
	        m_directory = "";
	    m_back_buffer = static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0)));

	    /* Setup Font Library */
	    FT_Init_FreeType(&m_library);

	    litehtml::font_metrics fm;
	    create_font(get_default_font_name(), get_default_font_size(), 400, litehtml::font_style(0), 0, &fm);

	    /* Clear the screen to white */
	    draw_rect(m_back_buffer, 0, 0, m_vinfo->xres, m_vinfo->yres, litehtml::web_color(0xff, 0xff, 0xff));
	}

	~uxmux_container(void) {
	    // printf("dtor ~uxmux_container\n");
	    if (!m_fonts.empty()) {
	        std::unordered_map<std::string, font_structure_t>::iterator it;
	        for (it=m_fonts.begin(); it!=m_fonts.end(); ++it){
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

	void swap_buffer(litehtml::uint_ptr hdc) {
	    int i;
	    for (i=0; i<(m_vinfo->yres_virtual * m_finfo->line_length)/4; i++) {
	        (reinterpret_cast<uint32_t*>(hdc))[i] = m_back_buffer[i];
	    }
	}

	void swap_buffer(litehtml::uint_ptr src_hdc, litehtml::uint_ptr dest_hdc, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo) {
	    int i;
	    for (i=0; i<(vinfo->yres_virtual * finfo->line_length)/4; i++) {
	        (reinterpret_cast<uint32_t*>(dest_hdc))[i] = reinterpret_cast<uint32_t*>(src_hdc)[i];
	    }
	}

	void draw_mouse(litehtml::uint_ptr hdc, int xpos, int ypos, unsigned char click) {
	    // printf("draw_mouse, at (%d, %d)\n", xpos, ypos);
	    if (m_cursor) {
	        draw_rect(hdc, xpos-1, ypos-4, 3, 9, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
	        draw_rect(hdc, xpos-4, ypos-1, 9, 3, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
	    } else {
	        draw_rect(hdc, xpos-1, ypos-2, 3, 5, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
	        draw_rect(hdc, xpos-2, ypos-1, 5, 3, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
	    }
	}

	void draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, const litehtml::web_color& color) {
	    draw_rect(hdc, rect.x, rect.y, rect.width, rect.height, color);
	}

	void draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, const litehtml::web_color& color) {
	    // printf("   draw_rect, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", xpos, ypos, width, height, static_cast<int>(color.red), static_cast<int>(color.green), static_cast<int>(color.blue));

	    long x, y;
	    for (x = xpos; x < xpos + width; x++) {
	        for (y = ypos; y < ypos + height; y++) {
	            if (x < 0 || y < 0 || x >= m_vinfo->xres || y >= m_vinfo->yres)
	                continue;
	            long location = (x+m_vinfo->xoffset)*(m_vinfo->bits_per_pixel/8) + (y+m_vinfo->yoffset)*m_finfo->line_length;
	            *(reinterpret_cast<uint32_t*>(hdc+location)) = static_cast<uint32_t>(color.red<<m_vinfo->red.offset | color.green<<m_vinfo->green.offset | color.blue<<m_vinfo->blue.offset);
	        }
	    }
	}

	void load_font(litehtml::uint_ptr hFont) {
	    font_structure_t* font_struct = reinterpret_cast<font_structure_t*>(hFont);
	    load_font(*font_struct);
	}

	void load_font(font_structure_t font_struct) {
	    m_face = font_struct.font;
	    if (!font_struct.valid || !m_face) {
	        font_struct.valid = false;
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

	bool check_new_page(){return m_new_page != "";}
	bool check_new_page_alt(){return m_new_page_alt != "";}
	uint32_t* get_back_buffer(){return m_back_buffer;}
	std::string get_directory(){return m_directory;}

	/* From abstract class "litehtml::document_container" in "litehtml/src/html.h"
	   see also: https://github.com/litehtml/litehtml/wiki/document_container */
	litehtml::uint_ptr create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) override;
	void delete_font(litehtml::uint_ptr hFont) override;
	int text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) override;
	void draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) override;
	int pt_to_px(int pt) override;
	int get_default_font_size() const override;
	const litehtml::tchar_t* get_default_font_name() const override;
	void draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) override;
	void load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) override;
	void get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) override;
	void draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) override;
	void draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) override;
	void set_caption(const litehtml::tchar_t* caption) override;
	void set_base_url(const litehtml::tchar_t* base_url) override;
	void link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) override;
	void on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) override;
	void set_cursor(const litehtml::tchar_t* cursor) override;
	void transform_text(litehtml::tstring& text, litehtml::text_transform tt) override;
	void import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) override;
	void set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y) override;
	void del_clip() override;
	void get_client_rect(litehtml::position& client) const override;
	std::shared_ptr<litehtml::element> create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) override;
	void get_media_features(litehtml::media_features& media) const override;
	void get_language(litehtml::tstring& language,litehtml::tstring& culture) const override;
};

#endif
