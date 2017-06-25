#include "uxmux_container.h"

/* See https://github.com/litehtml/litehtml/wiki/document_container */

uxmux_container::uxmux_container(std::string prefix, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo) {
    // printf("ctor uxmux_container\n");

    if (strcmp(prefix.c_str(),"")!=0)
        m_directory = prefix+"/";
    else
        m_directory = "";
    m_finfo = finfo;
    m_vinfo = vinfo;
    m_back_buffer = static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0)));

    m_cursor = false;
    m_new_page = "";
    m_new_page_alt = "";

    /* Setup Font Library */
    FT_Init_FreeType(&m_library);

    litehtml::font_metrics fm;
    m_default_font.font = 0;
    m_default_font.valid = false;
    create_font(get_default_font_name(), get_default_font_size(), 400, litehtml::font_style(0), 0, &fm);

    clear_screen();
}

uxmux_container::~uxmux_container(void) {
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

inline uint32_t uxmux_container::pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
    return static_cast<uint32_t>(r<<vinfo->red.offset | g<<vinfo->green.offset | b<<vinfo->blue.offset);
}

std::string uxmux_container::get_new_page() {
    // printf("get_new_page\n");
    if (check_new_page()) {
        std::string ret(m_new_page.c_str());
        m_new_page = "";
        return ret;
    }
    return 0;
}

std::string uxmux_container::get_new_page_alt() {
    // printf("get_new_page_alt\n");
    if (check_new_page_alt()) {
        std::string ret(m_new_page_alt.c_str());
        m_new_page_alt = "";
        return ret;
    }
    return 0;
}

void uxmux_container::clear_screen() {
    /* Clear the screen to white */
    draw_rect(m_back_buffer, 0, 0, m_vinfo->xres, m_vinfo->yres, litehtml::web_color(0xff, 0xff, 0xff));
}

void uxmux_container::swap_buffer(litehtml::uint_ptr hdc) {
    int i;
    for (i=0; i<(m_vinfo->yres_virtual * m_finfo->line_length)/4; i++) {
        (reinterpret_cast<uint32_t*>(hdc))[i] = m_back_buffer[i];
    }
}

void uxmux_container::swap_buffer(litehtml::uint_ptr src_hdc, litehtml::uint_ptr dest_hdc, struct fb_var_screeninfo *vinfo, struct fb_fix_screeninfo *finfo) {
    int i;
    for (i=0; i<(vinfo->yres_virtual * finfo->line_length)/4; i++) {
        (reinterpret_cast<uint32_t*>(dest_hdc))[i] = reinterpret_cast<uint32_t*>(src_hdc)[i];
    }
}

void uxmux_container::draw_mouse(litehtml::uint_ptr hdc, int xpos, int ypos, unsigned char click) {
    // printf("draw_mouse, at (%d, %d)\n", xpos, ypos);
    if (m_cursor) {
        draw_rect(hdc, xpos-1, ypos-4, 3, 9, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
        draw_rect(hdc, xpos-4, ypos-1, 9, 3, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
    } else {
        draw_rect(hdc, xpos-1, ypos-2, 3, 5, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
        draw_rect(hdc, xpos-2, ypos-1, 5, 3, litehtml::web_color(click&0x4?0xff:0, click&0x2?0xff:0, click&0x1?0xff:0));
    }
}

void uxmux_container::draw_rect(litehtml::uint_ptr hdc, const litehtml::position& rect, const litehtml::web_color& color) {
    draw_rect(hdc, rect.x, rect.y, rect.width, rect.height, color);
}

void uxmux_container::draw_rect(litehtml::uint_ptr hdc, int xpos, int ypos, int width, int height, const litehtml::web_color& color) {
    // printf("   draw_rect, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", xpos, ypos, width, height, static_cast<int>(color.red), static_cast<int>(color.green), static_cast<int>(color.blue));

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

void uxmux_container::load_font(litehtml::uint_ptr hFont) {
    font_structure_t* font_struct = reinterpret_cast<font_structure_t*>(hFont);
    load_font(*font_struct);
}

void uxmux_container::load_font(font_structure_t font_struct) {
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

litehtml::uint_ptr uxmux_container::create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm) {
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
            load_font(m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
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
                                load_font(m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
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
                                                load_font(m_default_font);
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
        // double angle = 0;
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

        fm->x_height = m_slot->bitmap.rows;

        // printf("      height=%d, ascent=%d, descent=%d, x_height=%d\n", fm->height, fm->ascent, fm->descent, fm->x_height);

        return reinterpret_cast<litehtml::uint_ptr>(&m_fonts[name+mod+std::to_string(decoration)+std::to_string(size)]);
    }

    return 0;
}

void uxmux_container::delete_font(litehtml::uint_ptr hFont) {
    // printf("delete_font\n");
}

int uxmux_container::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont) {
    // printf("text_width\n");

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

void uxmux_container::draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos) {
    // printf("draw_text: %s, at (%d, %d), size (%d, %d), color (%d, %d, %d)\n", text, pos.x, pos.y, pos.width, pos.height, static_cast<int>(color.red), static_cast<int>(color.green), static_cast<int>(color.blue));

    load_font(hFont);
    if (!m_face) return;

    // printf("   decoration: %d\n", m_face->generic.data);

    int xpos = pos.x;
    int ypos = pos.y;

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

        // printf("  bitmap_left=%d,  bitmap_top=%d, bitmap.width=%d, bitmap.rows=%d\n", m_slot->bitmap_left, m_slot->bitmap_top, m_slot->bitmap.width, m_slot->bitmap.rows);

        /* now, draw to our target surface (convert position) */
        int i, j, p, q;
        int targety = ypos + pos.height - m_slot->bitmap_top + ypos + m_face->size->metrics.descender/64;
        // printf("line height: %d\n", m_face->size->metrics.height/64);
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

int uxmux_container::pt_to_px(int pt) {
    // printf("pt_to_px: %d\n", pt);
    return pt;
}

int uxmux_container::get_default_font_size() const{
    // printf("get_default_font_size\n");
    return 16;
}

const litehtml::tchar_t* uxmux_container::get_default_font_name() const{
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
void uxmux_container::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker) {
    // printf("draw_list_marker %s at (%d, %d)\n", marker.image.c_str(), marker.pos.x, marker.pos.y);
    draw_rect(hdc, marker.pos.x, marker.pos.y, marker.pos.width, marker.pos.height, marker.color);
}

void uxmux_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready) {
    // printf("load_image: %s\n", src);
}

void uxmux_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz) {
    // printf("get_image_size: %s\n", src);
    m_image_loader.image_size((m_directory+src).c_str(), &sz.width, &sz.height);
}

void uxmux_container::draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg) {
    // printf("draw_background: %s at (%d, %d)\n", bg.image.c_str(), bg.position_x, bg.position_y);
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
void uxmux_container::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root) {
    // printf("draw_borders\n");
    draw_rect(hdc, draw_pos.x, draw_pos.y, draw_pos.width-borders.right.width, borders.top.width, borders.top.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y+draw_pos.height-borders.bottom.width, draw_pos.width-borders.right.width, borders.bottom.width, borders.bottom.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y, borders.left.width, draw_pos.height, borders.left.color);
    draw_rect(hdc, draw_pos.x+draw_pos.width-borders.right.width, draw_pos.y, borders.right.width, draw_pos.height, borders.right.color);
}

void uxmux_container::set_caption(const litehtml::tchar_t* caption) {
    // printf("set_caption: %s\n", caption);
}

void uxmux_container::set_base_url(const litehtml::tchar_t* base_url) {
    // printf("set_base_url\n");
}

void uxmux_container::link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el) {
    // printf("link: rel=%s, type=%s, href=%s\n", el->get_attr("rel"), el->get_attr("type"), el->get_attr("href"));
}

void uxmux_container::on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el) {
    // printf("on_anchor_click: %s\n", url);
    std::string filename = url;
    if (strcmp(filename.substr(filename.find_last_of(".") + 1).c_str(), "html") == 0) {
        m_new_page = filename;
        m_new_page_alt = "";
    } else if (filename != "") {
        m_new_page = filename+".html";
        m_new_page_alt = filename+"/index.html";
    }
}

void uxmux_container::set_cursor(const litehtml::tchar_t* cursor) {
    // printf("set_cursor: %s\n", cursor);
    if (strcmp(cursor, "auto") != 0)
        m_cursor = true;
    else
        m_cursor = false;
}

void uxmux_container::transform_text(litehtml::tstring& text, litehtml::text_transform tt) {
    // printf("transform_text: %s\n", text.c_str());
}

void uxmux_container::import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl) {
    // printf("import_css: base=%s, url=%s\n", baseurl.c_str(), url.c_str());

    std::ifstream t(m_directory+url);
    // printf("    %s, url=%s\n", m_directory.c_str(), url.c_str());
    std::stringstream buffer;
    buffer << t.rdbuf();

    // printf("\n%s\n\n", buffer.str().c_str());

    text = buffer.str();
}

void uxmux_container::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y) {
    // printf("set_clip\n");
}

void uxmux_container::del_clip() {
    // printf("del_clip\n");
}

void uxmux_container::get_client_rect(litehtml::position& client) const{
    // printf("get_client_rect (%d, %d)\n", m_vinfo->xres, m_vinfo->yres);
    client.x = 0;
    client.y = 0;
    client.width = m_vinfo->xres;
    client.height = m_vinfo->yres;
}

std::shared_ptr<litehtml::element> uxmux_container::create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc) {
    // printf("create_element: %s\n", tag_name);
    litehtml::element element(doc);
    element.set_tagName(tag_name);
    if (!attributes.empty()) {
        std::map<litehtml::tstring,litehtml::tstring>::const_iterator it;
        for (it=attributes.begin(); it!=attributes.end(); ++it){
            // printf("   set_attr: %s=%s\n", it->first.c_str(), it->second.c_str());
            element.set_attr(it->first.c_str(), it->second.c_str());
        }
    }
    return 0;
}

void uxmux_container::get_media_features(litehtml::media_features& media) const{
    // printf("get_media_features\n");
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

void uxmux_container::get_language(litehtml::tstring& language, litehtml::tstring& culture) const{
    // printf("get_language\n");
    language = _t("en");
    culture = _t("");
}
