/* vim: set ai et ts=4 sw=4: */
#ifndef __FONTS_H__
#define __FONTS_H__

#include <stdint.h>

typedef struct {
    const uint8_t width;
    uint8_t height;
    const uint16_t *data;
} FontDef;


extern FontDef Font_7x10;
extern FontDef Font_11x18;
extern FontDef Font_16x26;
extern FontDef Font_10x18_Menu;

/* UTF-8 helpers used by the display drivers. */
uint32_t Font_NextCodepoint(const char **text);
const uint16_t *Font_GetGlyph(FontDef font, uint32_t codepoint);
uint16_t Font_GetTextWidth(const char *text, FontDef font);

#endif // __FONTS_H__
