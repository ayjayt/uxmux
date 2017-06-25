#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <linux/fb.h>
#include <stdint.h>
#include <stdlib.h>

/* libpng16 */
# include <png.h>

#define FH_ERROR_OK     0
#define FH_ERROR_FILE   1   /* read/access error */
#define FH_ERROR_FORMAT 2   /* file format error */

class image_loader {
private:
    enum file_type_t {
        none,
        bmp,
        png,
        jpeg
    };
    file_type_t m_file_type;

    int m_x, m_y;
    int m_width, m_height;

    /* PNG */
    png_byte m_color_type;
    png_byte m_bit_depth;
    png_structp m_png_ptr;
    png_infop m_info_ptr;
    int m_number_of_passes;
    png_bytep* m_row_pointers;

    int load_png(const char* file_name);
    int png_size(const char* file_name, int *x, int *y);

public:
    image_loader(void);
    ~image_loader(void);

    int load_image(const char* file_name);
    int image_size(const char* file_name, int *x, int *y);
    int copy_to_framebuffer(void* hdc, struct fb_fix_screeninfo* finfo, struct fb_var_screeninfo* vinfo, int posx, int posy);
    void destroy();
};

#endif
