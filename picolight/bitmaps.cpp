#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "bitmaps.h"

static inline void * _allocmem(int amt)
{
//    return (void *) sbrk(amt);
    return calloc(amt,1);
}

bwbitmap_t *bwbm_create(int width, int height)
{
    bwbitmap_t *bwb;
    int stride = (width + 7) / 8;
    int totalbytes = (stride * height);

    bwb = (bwbitmap_t *) _allocmem(sizeof(bwbitmap_t) + totalbytes);

    bwb->bm_bits = (uint8_t *) (bwb+1);
    bwb->bm_ref = NULL;
    bwb->bm_stride = stride;
    bwb->bm_width = width;
    bwb->bm_height = height;

    bwbm_clear(bwb);

    return bwb;
}

void bwbm_clear(bwbitmap_t *bwb)
{
    memset(bwb->bm_bits, 0, bwb->bm_height * bwb->bm_stride);
}

void bwbm_invert(bwbitmap_t *bwb)
{
    int totalbytes = bwb->bm_height * bwb->bm_stride;
    int i;

    for (i = 0; i < totalbytes; i++) {
        bwb->bm_bits[i] = ~bwb->bm_bits[i];
    }
}

int bwbm_bltchar(bwbitmap_t *bwb, const bwfont_t *bwf, int x, int y, char c, int mode)
{
    // This is not going to win any awards for efficiency, but the bitmaps we
    // are dealing with are small, and the microcontrollers we have are very fast.
    unsigned int i,j;

    // Info about the *source* bitmaps.
    unsigned int charwid = (bwf->bwf_widths[(int)c]);  // width in bits
    unsigned int charbytes = (charwid + 7) / 8;
    unsigned int charhgt = (bwf->bwf_ascent + bwf->bwf_descent);
    uint8_t charmask;
    uint8_t leftmask, rightmask;
    unsigned int rowbits;
    const uint8_t *charbits = (bwf->bwf_bitmap + bwf->bwf_offsets[(int)c]);

    // Info about the *destination* bitmap.  Let's for now put the character's origin
    // as its upper left corner.  In the future we may want to place it on the bottom left
    // (with the descenders going below that line).
    uint8_t *bmbits = bwb->bm_bits + bwb->bm_stride*y + (x / 8);
    unsigned int bitoffset = (x & 7);           // the offset, in bits, from the left.

    // Iterate across the rows, pulling the byte from the font, shifting it into
    // position, and merging it into the destination bitmap.
    
    for (j = 0; j < charhgt; j++) {
        // Clip if we run off the bottom of the bitmap
        if (y >= bwb->bm_height) {
            break;
        }
        rowbits = charwid;
        for (i = 0; i < charbytes; i++) {
            uint8_t fontbits = charbits[i];
            charmask = 0xFF << ((rowbits >= 8) ? 0 : (8-rowbits));
            switch (mode) {
                case BWBM_MODE_OR:
                case BWBM_MODE_AND:
                    if (mode == BWBM_MODE_AND) fontbits ^= charmask;
                    bmbits[i] |= (fontbits >> bitoffset);
                    if (bitoffset != 0) {
                        bmbits[i+1] |= (fontbits << (8-bitoffset));
                    }
                    break;
                case BWBM_MODE_INVERT:
                    bmbits[i] ^= (fontbits >> bitoffset);
                    if (bitoffset != 0) {
                        bmbits[i+1] ^= (fontbits << (8-bitoffset));
                    }
                    break;
                case BWBM_MODE_REPLACE:
                    leftmask = 0xFF >> bitoffset;
                    rightmask = 0xFF << (8-bitoffset);
                    bmbits[i] = (bmbits[i] & ~leftmask) | (fontbits >> bitoffset);
                    if (bitoffset != 0) {
                        bmbits[i+1] = (bmbits[i+1] & ~rightmask) | (fontbits << (8-bitoffset));
                    }
                    break;
            }
                    
            bmbits[i] |= (fontbits >> bitoffset);
            if (bitoffset != 0) {
                bmbits[i+1] |= (fontbits << (8-bitoffset));
            }
            rowbits -= (rowbits >= 8) ? 8 : rowbits;
        }
        charbits += charbytes;
        bmbits += bwb->bm_stride;
        y++;
    }
    return charwid;
}

int bwbm_blttext(bwbitmap_t *bwb, const bwfont_t *bwf, int x, int y, const char *str, int mode)
{
    int ttlwid;
    int w;
    
    while (*str) {
        w = bwbm_bltchar(bwb,bwf,x,y,*str,mode);
        x += w;
        ttlwid += w;
        str++;
    }
    return ttlwid;
}

int bwbm_getbit(bwbitmap_t *bwb, int x, int y)
{
    uint8_t raw = bwb->bm_bits[(y*bwb->bm_stride) + (x / 8)];

    return ((0x80 >> (x & 7)) & raw) ? 1 : 0;
}


