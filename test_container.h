#include <linux/fb.h>
#include <sys/mman.h>
#include <unordered_map>
#include "litehtml.h"
#include "image_loader.h"

#include <ft2build.h>
#include FT_FREETYPE_H

class test_container : public litehtml::document_container {
private:
	std::unordered_map<std::string, litehtml::uint_ptr> m_fonts;
	bool m_delete_flag;

    image_loader m_image_loader;

	FT_Library m_library;
	FT_Face m_face;
	FT_Face m_default_face;
	FT_GlyphSlot m_slot;

	std::string m_directory;
	struct fb_fix_screeninfo* m_finfo;
	struct fb_var_screeninfo* m_vinfo;

public:
	uint32_t* m_back_buffer;

	// test_container(void);
	test_container(std::string prefix, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo);
	~test_container(void);

	inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo);
	void draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, litehtml::web_color color);
	void draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, litehtml::web_color color);
	void swap_buffer(litehtml::uint_ptr hdc);
	void load_font(litehtml::uint_ptr hFont);

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
