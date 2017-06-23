#include "test_container.h"

#include <iostream>
#include <fstream>
#include <math.h>

/* See https://github.com/litehtml/litehtml/wiki/document_container */

test_container::test_container(std::string prefix, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo) {
    std::cout << "ctor test_container" << std::endl;
    m_delete_flag = false;

    /* Setup Font Library */
    FT_Init_FreeType(&m_library);

    if (strcmp(prefix.c_str(),"")!=0)
        m_directory = prefix+"/";
    else
        m_directory = "";
    m_finfo = finfo;
    m_vinfo = vinfo;
    m_back_buffer = static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0)));

    /* Clear the screen to white */
    draw_rect(m_back_buffer, 0, 0, vinfo->xres, vinfo->yres, litehtml::web_color(0xff, 0xff, 0xff));
}

test_container::~test_container(void) {
	std::cout << "dtor ~test_container" << std::endl;
    if (m_delete_flag && m_library) {
        std::cout << "clean up FontLibrary" << std::endl;
        FT_Done_FreeType(m_library);
        m_library = 0;
    }
}

inline uint32_t test_container::pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
    return static_cast<uint32_t>(r<<vinfo->red.offset | g<<vinfo->green.offset | b<<vinfo->blue.offset);
}

void test_container::swap_buffer(litehtml::uint_ptr hdc) {
    int i;
    for (i=0; i<(m_vinfo->yres_virtual * m_finfo->line_length)/4; i++) {
        (reinterpret_cast<uint32_t*>(hdc))[i] = m_back_buffer[i];
    }
}

void test_container::draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, litehtml::web_color color) {
    draw_rect(hdc, rect.x, rect.y, rect.width, rect.height, color);
}

void test_container::draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, litehtml::web_color color) {
    std::cout << "   draw_rect, at (" << xpos << ", " << ypos << "), size (" << width << ", " << height << "), color (" << static_cast<int>(color.red) << ", " << static_cast<int>(color.green) << ", " << static_cast<int>(color.blue) << ")" << std::endl;

    long x, y;
    for (x = xpos; x < xpos + width; x++) {
        for (y = ypos; y < ypos + height; y++) {
            if (x < 0 || y < 0 || x >= m_vinfo->xres || y >= m_vinfo->yres)
                continue;
            long location = (x+m_vinfo->xoffset)*(m_vinfo->bits_per_pixel/8) + (y+m_vinfo->yoffset)*m_finfo->line_length;
            *(reinterpret_cast<uint32_t*>(hdc+location)) = pixel_color(color.red, color.green, color.blue, m_vinfo);
        }
    }
}

void test_container::load_font(litehtml::uint_ptr hFont) {
    m_face = static_cast<FT_Face>(hFont);
    if (!m_face) m_face = m_default_face;
    if (!m_face) return;
    m_slot = m_face->glyph;
}

litehtml::uint_ptr test_container::create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) {
	std::cout << "create_font: " << faceName << ", size="<< size << ", weight="<< weight << ", style="<< italic << ", decoration="<< decoration << std::endl;

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
        std::cout << "   Parsed Name : " << name << std::endl;

        std::string key = name;
        std::string keyalt = name;
        if (italic && weight>=600) {
            key+="-BoldItalic";
            keyalt+="-BoldOblique";
        } else if (weight>=600) {
            key+="-Bold";
            keyalt+="-Heavy";
        } else if (italic) {
            key+="-Italic";
            keyalt+="-Oblique";
        } else
            keyalt+="-Regular";

        /* If font already exists, return it*/
        if (m_fonts.count(key+std::to_string(decoration)+std::to_string(size)))
            return m_fonts[key+std::to_string(decoration)+std::to_string(size)];

        if (FT_New_Face(m_library, (m_directory+"fonts/"+key+".ttf").c_str(), 0, &m_face)) {
            std::cout << "   Error loading: fonts/" << key << ".tff" << std::endl << "   Looking in system instead.." << std::endl;
            if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+key+".ttf").c_str(), 0, &m_face)) {
                std::cout << "   Not found. Trying alternative: fonts/" << keyalt << ".tff" << std::endl;
                if (FT_New_Face(m_library, (m_directory+"fonts/"+keyalt+".ttf").c_str(), 0, &m_face)) {
                    std::cout << "   Error loading: fonts/" << keyalt << ".tff" << std::endl << "   Looking in system instead.." << std::endl;
                    if (FT_New_Face(m_library, ("/usr/share/fonts/truetype/dejavu/"+keyalt+".ttf").c_str(), 0, &m_face)) {
                        std::cout << "   WARNING: " << key << ".tff (alt " << keyalt << ".tff)" << " could not be found." << std::endl;
                        m_library = 0;
                        return 0;
                    }
                }
            }
        }
        FT_Set_Char_Size(m_face, 0, size*16, 300, 300);
        m_face->generic.data = reinterpret_cast<void*>(decoration);

        /* Save the first valid font to be used if other fonts are invalid*/
        if (!m_default_face) m_default_face = m_face;

        m_slot = m_face->glyph;

        m_fonts[key+std::to_string(decoration)+std::to_string(size)] = m_face;

        /* set up matrix */
        FT_Matrix matrix;
        // double angle = 0;
        matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
        matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
        matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
        matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

        fm->height = m_face->size->metrics.height/64;
        fm->ascent = m_face->size->metrics.ascender/64;
        fm->descent = m_face->size->metrics.descender/64;

        FT_Vector pen;
        pen.x = 0;
        pen.y = 0;

        FT_Set_Transform(m_face, &matrix, &pen);
        FT_Load_Char(m_face, 'x', FT_LOAD_RENDER);

        fm->x_height = m_slot->bitmap_top;

        std::cout << "   height=" << fm->height << ", ascent=" << fm->ascent << ", descent=" << fm->descent << ", x_height=" << fm->x_height << std::endl;

        return reinterpret_cast<litehtml::uint_ptr>(m_face);
    }

    return 0;
}

void test_container::delete_font(litehtml::uint_ptr hFont) {
    std::cout << "delete_font" << std::endl;
    if (hFont)
        FT_Done_Face(reinterpret_cast<FT_Face>(hFont));
    m_delete_flag = true;
}

int test_container::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) {
	// std::cout << "text_width" << std::endl;

    load_font(hFont);
    if (!m_face) return 0;

    int width;

    /* set up matrix */
    FT_Matrix matrix;
    // double angle = 0;
    matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

    FT_Vector pen;
    pen.x = 0;
    pen.y = 0;

    int n;
    for (n = 0; n < strlen(text); n++) {
        FT_Set_Transform(m_face, &matrix, &pen);
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(m_face, text[n], FT_LOAD_RENDER))
            continue;  /* ignore errors */

        if (n == strlen(text)-1)
            width = m_slot->bitmap_left+m_slot->advance.x/64;

        /* increment pen position */
        pen.x += m_slot->advance.x;
        pen.y += m_slot->advance.y;
    }

	return width;
}

void test_container::draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
	std::cout << "draw_text: " << text << ", at (" << pos.x << ", " << pos.y << "), size (" << pos.width << ", " << pos.height << "), color (" << static_cast<int>(color.red) << ", " << static_cast<int>(color.green) << ", " << static_cast<int>(color.blue) << ")" << std::endl;

    load_font(hFont);
    if (!m_face) return;

    std::cout << "   decoration: " << m_face->generic.data << std::endl;

    int xpos = pos.x;
    int ypos = pos.y + m_face->size->metrics.descender/32;

    /* Draw boxes where text should be */
    // if (strcmp(text, " ")==0) {
    //     draw_rect(hdc, xpos, ypos, pos.width, pos.height, litehtml::web_color(0xff, 0, 0));
    // } else
    //     draw_rect(hdc, xpos, ypos, pos.width, pos.height, litehtml::web_color(0xff, 0xff, 0));

    /* set up matrix */
    FT_Matrix matrix;
    // double angle = 0;
    matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

    FT_Vector pen;
    pen.x = xpos * 64;
    pen.y = ypos * 64;

    int n;
    for (n = 0; n < strlen(text); n++) {
        FT_Set_Transform(m_face, &matrix, &pen);
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(m_face, text[n], FT_LOAD_RENDER))
            continue;  /* ignore errors */

        // std::cout << "  bitmap_left=" << m_slot->bitmap_left << ",  bitmap_top" << m_slot->bitmap_top << ", bitmap.width" << m_slot->bitmap.width << ", bitmap.rows" << m_slot->bitmap.rows << std::endl;

        /* now, draw to our target surface (convert position) */
        int i, j, p, q;
        int targety = ypos + pos.height - m_slot->bitmap_top + ypos + m_face->size->metrics.descender/64;
        // std::cout << "line height: " << m_face->size->metrics.height/64 << std::endl;
        for (i = m_slot->bitmap_left, p = 0; i < m_slot->bitmap_left+m_slot->bitmap.width; i++, p++) {
            for (j = targety, q = 0; j < targety+m_slot->bitmap.rows; j++, q++) {
                if (i < 0 || j < 0 || i >= m_vinfo->xres || j >= m_vinfo->yres)
                   continue;
                long location = (i+m_vinfo->xoffset)*(m_vinfo->bits_per_pixel/8) + (j+m_vinfo->yoffset)*m_finfo->line_length;
                uint32_t col = m_slot->bitmap.buffer[q * static_cast<unsigned int>(m_slot->bitmap.width) + p];
                /* Split pixel color into RGB components */
                uint8_t col_r = col&0xff0000;
                uint8_t col_g = col&0xff00;
                uint8_t col_b = col&0xff;
                /* Find max of color components */
                uint8_t col_max;
                col_max = col_b > col_r ? col_b : col_r;
                col_max = col_max > col_g ? col_max : col_g;
                /* Draw the text in grayscale (usually black) */
                if (col) {
                    uint32_t target = *(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location));
                    /* Blend the text color into the background */
                    /* color = alpha * (src - dest) + dest */
                    col_r = (static_cast<double>(col_max)/static_cast<double>(0xff))*static_cast<double>(color.red-static_cast<double>((target>>16)&0xff))+((target>>16)&0xff);
                    col_g = (static_cast<double>(col_max)/static_cast<double>(0xff))*static_cast<double>(color.green-static_cast<double>((target>>8)&0xff))+((target>>8)&0xff);
                    col_b = (static_cast<double>(col_max)/static_cast<double>(0xff))*static_cast<double>(color.blue-static_cast<double>(target&0xff))+(target&0xff);
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
        draw_rect(hdc, xpos, ypos+pos.height-1, pos.width, 1, color);
    }

    /* draw strikethrough */
    if (reinterpret_cast<long>(m_face->generic.data)&litehtml::font_decoration_linethrough) {
        draw_rect(hdc, xpos, ypos+pos.height/2, pos.width, 1, color);
    }

    /* draw overline */
    if (reinterpret_cast<long>(m_face->generic.data)&litehtml::font_decoration_overline) {
        draw_rect(hdc, xpos, ypos, pos.width, 1, color);
    }
}

int test_container::pt_to_px(int pt) {
	std::cout << "pt_to_px: " << pt << std::endl;
	return pt;
}

int test_container::get_default_font_size() const{
	// std::cout << "get_default_font_size" << std::endl;
	return 16;
}

const litehtml::tchar_t* test_container::get_default_font_name() const{
	// std::cout << "get_default_font_name" << std::endl;
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
void test_container::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
	std::cout << "draw_list_marker " << marker.image << " at " << marker.pos.x << ", " << marker.pos.y << std::endl;
    draw_rect(hdc, marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height, marker.color);
}

void test_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) {
	std::cout << "load_image: " << src << std::endl;
}

void test_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) {
	std::cout << "get_image_size: " << src << std::endl;
    m_image_loader.image_size((m_directory+src).c_str(), &sz.width, &sz.height);
}

void test_container::draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) {
	std::cout << "draw_background: " << bg.image << " at " << bg.position_x << ", " << bg.position_y << std::endl;
    if (strcmp(bg.image.c_str(), "")!=0)
        if (!m_image_loader.load_image((m_directory+bg.image).c_str()))
            if (!m_image_loader.copy_to_framebuffer(hdc, m_finfo, m_vinfo, bg.position_x, bg.position_y))
                return;
    draw_rect(hdc, bg.border_box.x, bg.border_box.y, bg.border_box.width, bg.border_box.height, bg.color);
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
void test_container::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
	std::cout << "draw_borders" << std::endl;
    draw_rect(hdc, draw_pos.x, draw_pos.y, draw_pos.width-borders.right.width, borders.top.width, borders.top.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y+draw_pos.height-borders.bottom.width, draw_pos.width-borders.right.width, borders.bottom.width, borders.bottom.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y, borders.left.width, draw_pos.height, borders.left.color);
    draw_rect(hdc, draw_pos.x+draw_pos.width-borders.right.width, draw_pos.y, borders.right.width, draw_pos.height, borders.right.color);
}

void test_container::set_caption(const litehtml::tchar_t* caption) {
	std::cout << "set_caption: " << caption << std::endl;
}

void test_container::set_base_url(const litehtml::tchar_t* base_url) {
	std::cout << "set_base_url" << std::endl;
}

void test_container::link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) {
	std::cout << "link" << std::endl;
}

void test_container::on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) {
	std::cout << "on_anchor_click" << std::endl;
}

void test_container::set_cursor(const litehtml::tchar_t* cursor) {
	std::cout << "set_cursor" << std::endl;
}

void test_container::transform_text(litehtml::tstring& text, litehtml::text_transform tt) {
	std::cout << "transform_text: " << text << std::endl;
}

void test_container::import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) {
	std::cout << "import_css: base=" << baseurl << ", url=" << url << std::endl;

    std::ifstream t(m_directory+url);
    std::cout << "    " << m_directory << ", url=" << url << std::endl;
    std::stringstream buffer;
    buffer << t.rdbuf();

    // std::cout << std::endl << buffer.str() << std::endl << std::endl;

    text = buffer.str();
}

void test_container::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y) {
	std::cout << "set_clip" << std::endl;
}

void test_container::del_clip() {
	std::cout << "del_clip" << std::endl;
}

void test_container::get_client_rect(litehtml::position& client) const{
	// std::cout << "get_client_rect (" << m_vinfo->xres << ", " << m_vinfo->yres << ")" << std::endl;
    client.x = 0;
    client.y = 0;
    client.width = m_vinfo->xres;
    client.height = m_vinfo->yres;
}

std::shared_ptr<litehtml::element> test_container::create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) {
	std::cout << "create_element: " << tag_name << std::endl;
	return 0;
}

void test_container::get_media_features(litehtml::media_features& media) const{
	std::cout << "get_media_features" << std::endl;
    litehtml::position client;
    get_client_rect(client);
    media.type = litehtml::media_type_screen;
    media.width = client.width;
    media.height = client.height;
    media.device_width = m_vinfo->xres;
    media.device_height = m_vinfo->yres;
    media.color = 8;
    media.monochrome = 0;
    media.color_index = 256;
    media.resolution = 96;
}

void test_container::get_language(litehtml::tstring& language, litehtml::tstring& culture) const{
	std::cout << "get_language" << std::endl;
    language = _t("en");
    culture = _t("");
}
