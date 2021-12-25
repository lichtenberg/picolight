

#ifndef __BITMAPS_H
#define __BITMAPS_H

typedef struct bwbitmap_s {
    uint8_t *bm_bits;
    void *bm_ref;
    int bm_stride;
    int bm_width;
    int bm_height;
} bwbitmap_t;

#define BWFONT_NAMESIZE 17
#define BWFONT_FACESIZE  3
typedef struct bwfont_s {
    char bwf_name[BWFONT_NAMESIZE];             // Name "Helvetica"
    char bwf_face[BWFONT_FACESIZE];             // Face "B", "I", "BI", ...
    uint8_t bwf_size;                           // Font's nominal height (points)
    uint8_t bwf_ascent;                         // pixels above the baseline
    uint8_t bwf_descent;                        // pixels below the baseline  (bitmap height = ascent+descent)
    uint8_t bwf_nchars;                         // number of characters in font
    const uint8_t *bwf_widths;                        // widths of each character, indexed by character
    const uint16_t *bwf_offsets;                      // offsets of each character into the bitmap
    unsigned int bwf_bmsize;                    // total bitmap size in bytes
    const uint8_t *bwf_bitmap;                        // pointer to font bitmap data
} bwfont_t;

#define BWBM_MODE_OR            0
#define BWBM_MODE_AND           1
#define BWBM_MODE_INVERT        2
#define BWBM_MODE_REPLACE       3

bwbitmap_t *bwbm_create(int width, int height);
void bwbm_clear(bwbitmap_t *bwb);
void bwbm_invert(bwbitmap_t *bwb);
int bwbm_bltchar(bwbitmap_t *bwb, const bwfont_t *bwf, int x, int y, char c, int mode);
int bwbm_getbit(bwbitmap_t *bwb, int x, int y);
int bwbm_blttext(bwbitmap_t *bwb, const bwfont_t *bwf, int x, int y, const char *str, int mode);
#endif
