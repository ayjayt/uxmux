#include "test_container.h"

#include <iostream>
#include <math.h>

/* See https://github.com/litehtml/litehtml/wiki/document_container */

test_container::test_container(struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo){
    std::cout << "ctor test_container" << std::endl;

    /* Setup Font Library */
    FT_Init_FreeType(&m_library);
    FT_New_Face(m_library, "/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf", 0, &m_face);
    FT_Set_Char_Size(m_face, 0, get_default_font_size()/4*64, 300, 300);
    m_slot = m_face->glyph;

    m_finfo = finfo;
    m_vinfo = vinfo;
    m_back_buffer = static_cast<uint32_t*>(mmap(0, vinfo->yres_virtual * finfo->line_length, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, off_t(0)));

    /* Clear the screen to white */
    draw_rect(m_back_buffer, 0, 0, vinfo->xres, vinfo->yres, litehtml::web_color(0xff, 0xff, 0xff));
}

test_container::~test_container(void){
	std::cout << "dtor ~test_container" << std::endl;

    FT_Done_Face(m_face);
    FT_Done_FreeType(m_library);
}

inline uint32_t test_container::pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo) {
    return static_cast<uint32_t>(r<<vinfo->red.offset | g<<vinfo->green.offset | b<<vinfo->blue.offset);
}

void test_container::swap_buffer(litehtml::uint_ptr hdc) {
    int i;
    // draw_rect(m_back_buffer, 0, 0, 16, 16, litehtml::web_color(0xff,0,0));
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
    for (x = xpos; x < width; x++) {
        for (y = ypos; y < height; y++) {
            long location = (x+m_vinfo->xoffset)*(m_vinfo->bits_per_pixel/8) + (y+m_vinfo->yoffset)*m_finfo->line_length;
            *(reinterpret_cast<uint32_t*>(hdc+location)) = pixel_color(color.red, color.green, color.blue, m_vinfo);
        }
    }
}

litehtml::uint_ptr test_container::create_font(const litehtml::tchar_t* faceName, int size, int weight, litehtml::font_style italic, unsigned int decoration, litehtml::font_metrics* fm){
	std::cout << "create_font: " << faceName << ", size="<< size << ", weight="<< weight << ", style="<< italic << ", decoration="<< decoration << std::endl;

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
    return 0;
}

void test_container::delete_font(litehtml::uint_ptr hFont){
    std::cout << "delete_font" << std::endl;
}

int test_container::text_width(const litehtml::tchar_t* text, litehtml::uint_ptr hFont){
	std::cout << "text_width" << std::endl;

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

void test_container::draw_text(litehtml::uint_ptr hdc, const litehtml::tchar_t* text, litehtml::uint_ptr hFont, litehtml::web_color color, const litehtml::position& pos){
	std::cout << "draw_text: " << text << ", at (" << pos.x << ", " << pos.y << "), size (" << pos.width << ", " << pos.height << ")" << std::endl;
    if (strcmp(text, " ")==0) {
        draw_rect(hdc, pos.x, pos.y, pos.width, pos.height, litehtml::web_color(0xff, 0, 0));
    } else
        draw_rect(hdc, pos.x, pos.y, pos.width, pos.height, litehtml::web_color(0xff, 0xff, 0));

    /* set up matrix */
    FT_Matrix matrix;
    // double angle = 0;
    matrix.xx = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);
    matrix.xy = 0;//(FT_Fixed)(-sin(angle) * 0x10000L);
    matrix.yx = 0;//(FT_Fixed)(sin(angle) * 0x10000L);
    matrix.yy = 0x10000L;//(FT_Fixed)(cos(angle) * 0x10000L);

    FT_Vector pen;
    pen.x = pos.x * 64;
    pen.y = pos.y * 64;

    int n;
    for (n = 0; n < strlen(text); n++) {
        FT_Set_Transform(m_face, &matrix, &pen);
        /* load glyph image into the slot (erase previous one) */
        if (FT_Load_Char(m_face, text[n], FT_LOAD_RENDER))
            continue;  /* ignore errors */

        std::cout << "  bitmap_left=" << m_slot->bitmap_left << ",  bitmap_top" << m_slot->bitmap_top << ", bitmap.width" << m_slot->bitmap.width << ", bitmap.rows" << m_slot->bitmap.rows << std::endl;

        /* now, draw to our target surface (convert position) */
        int i, j, p, q;
        int targety = pos.y + pos.height - m_slot->bitmap_top + pos.y;
        std::cout << "line height: " << m_face->size->metrics.height/64 << std::endl;
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
                col_b = col_b > col_r ? col_b : col_r;
                col_b = col_b > col_g ? col_b : col_g;
                col_b = col_b > col_r ? col_b : col_r;
                /* Draw the text in grayscale (usually black) */
                *(reinterpret_cast<uint32_t*>(reinterpret_cast<long>(hdc)+location)) = col ? ((0xff-col_b)<<16)|((0xff-col_b)<<8)|((0xff-col_b)) : 0xffffff;
            }
        }

        /* increment pen position */
        pen.x += m_slot->advance.x;
        pen.y += m_slot->advance.y;
    }
}

int test_container::pt_to_px(int pt){
	std::cout << "pt_to_px: " << pt << std::endl;
	return pt;
}

int test_container::get_default_font_size() const{
	std::cout << "get_default_font_size" << std::endl;
	return 16;
}

const litehtml::tchar_t* test_container::get_default_font_name() const{
	std::cout << "get_default_font_name" << std::endl;
    return "DejaVuSerif";
}

void test_container::draw_list_marker(litehtml::uint_ptr hdc, const litehtml::list_marker& marker){
	std::cout << "draw_list_marker" << std::endl;
}

void test_container::load_image(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready){
	std::cout << "load_image" << std::endl;
}

void test_container::get_image_size(const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz){
	std::cout << "get_image_size" << std::endl;
}

void test_container::draw_background(litehtml::uint_ptr hdc, const litehtml::background_paint& bg){
	std::cout << "draw_background" << std::endl;
}

void test_container::draw_borders(litehtml::uint_ptr hdc, const litehtml::borders& borders, const litehtml::position& draw_pos, bool root){
	std::cout << "draw_borders" << std::endl;
    draw_rect(hdc, draw_pos.x, draw_pos.y, draw_pos.width, borders.top.width, borders.top.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y+draw_pos.height, draw_pos.width, borders.bottom.width, borders.bottom.color);
    draw_rect(hdc, draw_pos.x, draw_pos.y, borders.left.width, draw_pos.height, borders.left.color);
    draw_rect(hdc, draw_pos.x+draw_pos.width, draw_pos.y, borders.right.width, draw_pos.height, borders.right.color);
}

void test_container::set_caption(const litehtml::tchar_t* caption){
	std::cout << "set_caption: " << caption << std::endl;
}

void test_container::set_base_url(const litehtml::tchar_t* base_url){
	std::cout << "set_base_url" << std::endl;
}

void test_container::link(const std::shared_ptr<litehtml::document>& doc, const litehtml::element::ptr& el){
	std::cout << "link" << std::endl;
}

void test_container::on_anchor_click(const litehtml::tchar_t* url, const litehtml::element::ptr& el){
	std::cout << "on_anchor_click" << std::endl;
}

void test_container::set_cursor(const litehtml::tchar_t* cursor){
	std::cout << "set_cursor" << std::endl;
}

void test_container::transform_text(litehtml::tstring& text, litehtml::text_transform tt){
	std::cout << "transform_text: " << text << std::endl;
}

void test_container::import_css(litehtml::tstring& text, const litehtml::tstring& url, litehtml::tstring& baseurl){
	std::cout << "import_css: base=" << baseurl << ", url=" << url << std::endl;
}

void test_container::set_clip(const litehtml::position& pos, const litehtml::border_radiuses& bdr_radius, bool valid_x, bool valid_y){
	std::cout << "set_clip" << std::endl;
}

void test_container::del_clip(){
	std::cout << "del_clip" << std::endl;
}

void test_container::get_client_rect(litehtml::position& client) const{
	std::cout << "get_client_rect (" << m_vinfo->xres << ", " << m_vinfo->yres << ")" << std::endl;
    client.x = 0;
    client.y = 0;
    client.width = m_vinfo->xres;
    client.height = m_vinfo->yres;
}

std::shared_ptr<litehtml::element> test_container::create_element(const litehtml::tchar_t* tag_name, const litehtml::string_map& attributes, const std::shared_ptr<litehtml::document>& doc){
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
