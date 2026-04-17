#ifndef MICROPY_INCLUDED_SPI_DISPLAYS_FONTS_H
#define MICROPY_INCLUDED_SPI_DISPLAYS_FONTS_H

#include <stdint.h>

// Чистые структуры формата Adafruit GFX
typedef struct {
    uint16_t bitmapOffset;     // Смещение в массиве битмапов
    uint8_t  width;            // Ширина символа
    uint8_t  height;           // Высота символа
    uint8_t  xAdvance;         // Дистанция до следующего символа
    int8_t   xOffset;          // Сдвиг по X
    int8_t   yOffset;          // Сдвиг по Y (относительно Baseline)
} GFXglyph;

typedef struct {
    uint8_t  *bitmap;          // Указатель на массив битмапов
    GFXglyph *glyph;           // Указатель на таблицу параметров символов
    uint16_t   first;          // Код первого символа
    uint16_t   last;           // Код последнего символа
    uint8_t   yAdvance;        // Высота строки
} GFXfont;

// Объявляем шрифты, чтобы их видел display.c
extern const GFXfont Font_L_6;
extern const GFXfont Font_C_6;
extern const GFXfont Font_L_8;
extern const GFXfont Font_C_8;
extern const GFXfont Font_L_12;
extern const GFXfont Font_C_12;
extern const GFXfont Font_L_16;
extern const GFXfont Font_C_16;

#endif // MICROPY_INCLUDED_SPI_DISPLAYS_FONTS_H