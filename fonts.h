#ifndef MICROPY_INCLUDED_SPI_DISPLAYS_FONTS_H
#define MICROPY_INCLUDED_SPI_DISPLAYS_FONTS_H

#include <stdint.h>

typedef struct {
    uint16_t bitmapOffset;
    uint8_t  width;
    uint8_t  height;
    uint8_t  xAdvance;
    int8_t   xOffset;
    int8_t   yOffset;
} GFXglyph;

typedef struct {
    uint8_t  *bitmap;
    GFXglyph *glyph;
    uint16_t   first;
    uint16_t   last;
    uint8_t   yAdvance;
} GFXfont;

extern const GFXfont Font_L_6;
extern const GFXfont Font_C_6;
extern const GFXfont Font_L_8;
extern const GFXfont Font_C_8;
extern const GFXfont Font_L_12;
extern const GFXfont Font_C_12;
extern const GFXfont Font_L_16;
extern const GFXfont Font_C_16;
extern const GFXfont Font_L_20;
extern const GFXfont Font_C_20;
extern const GFXfont Font_L_24;
extern const GFXfont Font_C_24;

#endif