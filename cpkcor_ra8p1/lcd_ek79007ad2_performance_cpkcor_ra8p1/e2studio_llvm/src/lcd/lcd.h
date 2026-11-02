#ifndef __LCD_H
#define __LCD_H

#include <stdbool.h>
#include <stdint.h>

#ifndef LCD_EN_PERI_GLCDC
#define LCD_EN_PERI_GLCDC	0
#endif

#ifndef LCD_EN_PERI_I2C
#define LCD_EN_PERI_I2C		0
#endif

#ifndef LCD_EN_PERI_MIPI
#define LCD_EN_PERI_MIPI	0
#endif

#ifndef LCD_EN_PERI_SCI_SPI
#define LCD_EN_PERI_SCI_SPI	0
#endif

#ifndef LCD_EN_PERI_SPI
#define LCD_EN_PERI_SPI		0
#endif

#if LCD_EN_PERI_GLCDC
#ifndef LCD_EN_EK79001
#define LCD_EN_EK79001      1
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_EK79007AD2
#define LCD_EN_EK79007AD2   1
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_ILI9881C
#define LCD_EN_ILI9881C     1
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_MIPI
#ifndef LCD_EN_JD9365HD
#define LCD_EN_JD9365HD     0
#endif
#endif

#if LCD_EN_PERI_GLCDC || LCD_EN_PERI_MIPI || LCD_EN_PERI_SCI_SPI
#ifndef LCD_EN_ST7796U
#define LCD_EN_ST7796U      0
#endif
#endif

#if LCD_EN_PERI_GLCDC && LCD_EN_PERI_I2C
#ifndef LCD_EN_TFP410PAPR
#define LCD_EN_TFP410PAPR   1
#endif
#endif

#define LCD_COLOR_ARGB1555_BLACK		0x8000
#define LCD_COLOR_ARGB1555_BLUE			0x801F
#define LCD_COLOR_ARGB1555_GREEN		0x83E0
#define LCD_COLOR_ARGB1555_GREY			0xC210
#define LCD_COLOR_ARGB1555_LIGHT_BLUE	0x83FF
#define LCD_COLOR_ARGB1555_ORANGE		0xFD80
#define LCD_COLOR_ARGB1555_PURPLE		0xCC1F
#define LCD_COLOR_ARGB1555_RED			0xFC00
#define LCD_COLOR_ARGB1555_WHITE		0xFFFF
#define LCD_COLOR_ARGB1555_YELLOW		0xFFE0

#define LCD_COLOR_ARGB4444_BLACK		0xF000
#define LCD_COLOR_ARGB4444_BLUE			0xF00F
#define LCD_COLOR_ARGB4444_GREEN		0xF0F0
#define LCD_COLOR_ARGB4444_GREY			0xF888
#define LCD_COLOR_ARGB4444_LIGHT_BLUE	0xF0FF
#define LCD_COLOR_ARGB4444_ORANGE		0xFF60
#define LCD_COLOR_ARGB4444_PURPLE		0xF90F
#define LCD_COLOR_ARGB4444_RED			0xFF00
#define LCD_COLOR_ARGB4444_WHITE		0xFFFF
#define LCD_COLOR_ARGB4444_YELLOW		0xFFF0

#define LCD_COLOR_ARGB8888_BLACK		0x00000000
#define LCD_COLOR_ARGB8888_BLUE			0xFF0000FF
#define LCD_COLOR_ARGB8888_GREEN		0xFF00FF00
#define LCD_COLOR_ARGB8888_GREY			0xFF808080
#define LCD_COLOR_ARGB8888_LIGHT_BLUE	0xFF00FFFF
#define LCD_COLOR_ARGB8888_ORANGE		0xFFFF6600
#define LCD_COLOR_ARGB8888_PURPLE		0xFF9900FF
#define LCD_COLOR_ARGB8888_RED			0xFFFF0000
#define LCD_COLOR_ARGB8888_WHITE		0xFFFFFFFF
#define LCD_COLOR_ARGB8888_YELLOW		0xFFFFFF00

/* Indices into sc_default_clut8; comments show the opaque ARGB8888 values.
 * RGB_RRGGBB names specify exact channel values; GREY_XX has R = G = B = XX.
 * Common color names follow the RGB values of the other LCD color formats.
 */
#define LCD_COLOR_CLUT8_BLACK           0x00 /* 0xFF000000 */
#define LCD_COLOR_CLUT8_BLUE            0x05 /* 0xFF0000FF */
#define LCD_COLOR_CLUT8_GREEN           0x1E /* 0xFF00FF00 */
#define LCD_COLOR_CLUT8_GREY            0xEC /* 0xFF808080 */
#define LCD_COLOR_CLUT8_GREY_08         0xD8 /* 0xFF080808 */
#define LCD_COLOR_CLUT8_GREY_0E         0xD9 /* 0xFF0E0E0E */
#define LCD_COLOR_CLUT8_GREY_14         0xDA /* 0xFF141414 */
#define LCD_COLOR_CLUT8_GREY_1A         0xDB /* 0xFF1A1A1A */
#define LCD_COLOR_CLUT8_GREY_20         0xDC /* 0xFF202020 */
#define LCD_COLOR_CLUT8_GREY_26         0xDD /* 0xFF262626 */
#define LCD_COLOR_CLUT8_GREY_2C         0xDE /* 0xFF2C2C2C */
#define LCD_COLOR_CLUT8_GREY_32         0xDF /* 0xFF323232 */
#define LCD_COLOR_CLUT8_GREY_38         0xE0 /* 0xFF383838 */
#define LCD_COLOR_CLUT8_GREY_3E         0xE1 /* 0xFF3E3E3E */
#define LCD_COLOR_CLUT8_GREY_44         0xE2 /* 0xFF444444 */
#define LCD_COLOR_CLUT8_GREY_4A         0xE3 /* 0xFF4A4A4A */
#define LCD_COLOR_CLUT8_GREY_50         0xE4 /* 0xFF505050 */
#define LCD_COLOR_CLUT8_GREY_56         0xE5 /* 0xFF565656 */
#define LCD_COLOR_CLUT8_GREY_5C         0xE6 /* 0xFF5C5C5C */
#define LCD_COLOR_CLUT8_GREY_62         0xE7 /* 0xFF626262 */
#define LCD_COLOR_CLUT8_GREY_68         0xE8 /* 0xFF686868 */
#define LCD_COLOR_CLUT8_GREY_6E         0xE9 /* 0xFF6E6E6E */
#define LCD_COLOR_CLUT8_GREY_74         0xEA /* 0xFF747474 */
#define LCD_COLOR_CLUT8_GREY_7A         0xEB /* 0xFF7A7A7A */
#define LCD_COLOR_CLUT8_GREY_86         0xED /* 0xFF868686 */
#define LCD_COLOR_CLUT8_GREY_8C         0xEE /* 0xFF8C8C8C */
#define LCD_COLOR_CLUT8_GREY_92         0xEF /* 0xFF929292 */
#define LCD_COLOR_CLUT8_GREY_98         0xF0 /* 0xFF989898 */
#define LCD_COLOR_CLUT8_GREY_9E         0xF1 /* 0xFF9E9E9E */
#define LCD_COLOR_CLUT8_GREY_A4         0xF2 /* 0xFFA4A4A4 */
#define LCD_COLOR_CLUT8_GREY_AA         0xF3 /* 0xFFAAAAAA */
#define LCD_COLOR_CLUT8_GREY_B0         0xF4 /* 0xFFB0B0B0 */
#define LCD_COLOR_CLUT8_GREY_B6         0xF5 /* 0xFFB6B6B6 */
#define LCD_COLOR_CLUT8_GREY_BC         0xF6 /* 0xFFBCBCBC */
#define LCD_COLOR_CLUT8_GREY_C2         0xF7 /* 0xFFC2C2C2 */
#define LCD_COLOR_CLUT8_GREY_C8         0xF8 /* 0xFFC8C8C8 */
#define LCD_COLOR_CLUT8_GREY_CE         0xF9 /* 0xFFCECECE */
#define LCD_COLOR_CLUT8_GREY_D4         0xFA /* 0xFFD4D4D4 */
#define LCD_COLOR_CLUT8_GREY_DA         0xFB /* 0xFFDADADA */
#define LCD_COLOR_CLUT8_GREY_E0         0xFC /* 0xFFE0E0E0 */
#define LCD_COLOR_CLUT8_GREY_E6         0xFD /* 0xFFE6E6E6 */
#define LCD_COLOR_CLUT8_GREY_EC         0xFE /* 0xFFECECEC */
#define LCD_COLOR_CLUT8_GREY_F2         0xFF /* 0xFFF2F2F2 */
#define LCD_COLOR_CLUT8_LIGHT_BLUE      0x23 /* 0xFF00FFFF */
#define LCD_COLOR_CLUT8_MAGENTA         0xB9 /* 0xFFFF00FF */
#define LCD_COLOR_CLUT8_ORANGE          0xC0 /* 0xFFFF6600 */
#define LCD_COLOR_CLUT8_PURPLE          0x71 /* 0xFF9900FF */
#define LCD_COLOR_CLUT8_RED             0xB4 /* 0xFFFF0000 */
#define LCD_COLOR_CLUT8_RGB_000033      0x01 /* 0xFF000033 */
#define LCD_COLOR_CLUT8_RGB_000066      0x02 /* 0xFF000066 */
#define LCD_COLOR_CLUT8_RGB_000099      0x03 /* 0xFF000099 */
#define LCD_COLOR_CLUT8_RGB_0000CC      0x04 /* 0xFF0000CC */
#define LCD_COLOR_CLUT8_RGB_003300      0x06 /* 0xFF003300 */
#define LCD_COLOR_CLUT8_RGB_003333      0x07 /* 0xFF003333 */
#define LCD_COLOR_CLUT8_RGB_003366      0x08 /* 0xFF003366 */
#define LCD_COLOR_CLUT8_RGB_003399      0x09 /* 0xFF003399 */
#define LCD_COLOR_CLUT8_RGB_0033CC      0x0A /* 0xFF0033CC */
#define LCD_COLOR_CLUT8_RGB_0033FF      0x0B /* 0xFF0033FF */
#define LCD_COLOR_CLUT8_RGB_006600      0x0C /* 0xFF006600 */
#define LCD_COLOR_CLUT8_RGB_006633      0x0D /* 0xFF006633 */
#define LCD_COLOR_CLUT8_RGB_006666      0x0E /* 0xFF006666 */
#define LCD_COLOR_CLUT8_RGB_006699      0x0F /* 0xFF006699 */
#define LCD_COLOR_CLUT8_RGB_0066CC      0x10 /* 0xFF0066CC */
#define LCD_COLOR_CLUT8_RGB_0066FF      0x11 /* 0xFF0066FF */
#define LCD_COLOR_CLUT8_RGB_009900      0x12 /* 0xFF009900 */
#define LCD_COLOR_CLUT8_RGB_009933      0x13 /* 0xFF009933 */
#define LCD_COLOR_CLUT8_RGB_009966      0x14 /* 0xFF009966 */
#define LCD_COLOR_CLUT8_RGB_009999      0x15 /* 0xFF009999 */
#define LCD_COLOR_CLUT8_RGB_0099CC      0x16 /* 0xFF0099CC */
#define LCD_COLOR_CLUT8_RGB_0099FF      0x17 /* 0xFF0099FF */
#define LCD_COLOR_CLUT8_RGB_00CC00      0x18 /* 0xFF00CC00 */
#define LCD_COLOR_CLUT8_RGB_00CC33      0x19 /* 0xFF00CC33 */
#define LCD_COLOR_CLUT8_RGB_00CC66      0x1A /* 0xFF00CC66 */
#define LCD_COLOR_CLUT8_RGB_00CC99      0x1B /* 0xFF00CC99 */
#define LCD_COLOR_CLUT8_RGB_00CCCC      0x1C /* 0xFF00CCCC */
#define LCD_COLOR_CLUT8_RGB_00CCFF      0x1D /* 0xFF00CCFF */
#define LCD_COLOR_CLUT8_RGB_00FF33      0x1F /* 0xFF00FF33 */
#define LCD_COLOR_CLUT8_RGB_00FF66      0x20 /* 0xFF00FF66 */
#define LCD_COLOR_CLUT8_RGB_00FF99      0x21 /* 0xFF00FF99 */
#define LCD_COLOR_CLUT8_RGB_00FFCC      0x22 /* 0xFF00FFCC */
#define LCD_COLOR_CLUT8_RGB_330000      0x24 /* 0xFF330000 */
#define LCD_COLOR_CLUT8_RGB_330033      0x25 /* 0xFF330033 */
#define LCD_COLOR_CLUT8_RGB_330066      0x26 /* 0xFF330066 */
#define LCD_COLOR_CLUT8_RGB_330099      0x27 /* 0xFF330099 */
#define LCD_COLOR_CLUT8_RGB_3300CC      0x28 /* 0xFF3300CC */
#define LCD_COLOR_CLUT8_RGB_3300FF      0x29 /* 0xFF3300FF */
#define LCD_COLOR_CLUT8_RGB_333300      0x2A /* 0xFF333300 */
#define LCD_COLOR_CLUT8_RGB_333333      0x2B /* 0xFF333333 */
#define LCD_COLOR_CLUT8_RGB_333366      0x2C /* 0xFF333366 */
#define LCD_COLOR_CLUT8_RGB_333399      0x2D /* 0xFF333399 */
#define LCD_COLOR_CLUT8_RGB_3333CC      0x2E /* 0xFF3333CC */
#define LCD_COLOR_CLUT8_RGB_3333FF      0x2F /* 0xFF3333FF */
#define LCD_COLOR_CLUT8_RGB_336600      0x30 /* 0xFF336600 */
#define LCD_COLOR_CLUT8_RGB_336633      0x31 /* 0xFF336633 */
#define LCD_COLOR_CLUT8_RGB_336666      0x32 /* 0xFF336666 */
#define LCD_COLOR_CLUT8_RGB_336699      0x33 /* 0xFF336699 */
#define LCD_COLOR_CLUT8_RGB_3366CC      0x34 /* 0xFF3366CC */
#define LCD_COLOR_CLUT8_RGB_3366FF      0x35 /* 0xFF3366FF */
#define LCD_COLOR_CLUT8_RGB_339900      0x36 /* 0xFF339900 */
#define LCD_COLOR_CLUT8_RGB_339933      0x37 /* 0xFF339933 */
#define LCD_COLOR_CLUT8_RGB_339966      0x38 /* 0xFF339966 */
#define LCD_COLOR_CLUT8_RGB_339999      0x39 /* 0xFF339999 */
#define LCD_COLOR_CLUT8_RGB_3399CC      0x3A /* 0xFF3399CC */
#define LCD_COLOR_CLUT8_RGB_3399FF      0x3B /* 0xFF3399FF */
#define LCD_COLOR_CLUT8_RGB_33CC00      0x3C /* 0xFF33CC00 */
#define LCD_COLOR_CLUT8_RGB_33CC33      0x3D /* 0xFF33CC33 */
#define LCD_COLOR_CLUT8_RGB_33CC66      0x3E /* 0xFF33CC66 */
#define LCD_COLOR_CLUT8_RGB_33CC99      0x3F /* 0xFF33CC99 */
#define LCD_COLOR_CLUT8_RGB_33CCCC      0x40 /* 0xFF33CCCC */
#define LCD_COLOR_CLUT8_RGB_33CCFF      0x41 /* 0xFF33CCFF */
#define LCD_COLOR_CLUT8_RGB_33FF00      0x42 /* 0xFF33FF00 */
#define LCD_COLOR_CLUT8_RGB_33FF33      0x43 /* 0xFF33FF33 */
#define LCD_COLOR_CLUT8_RGB_33FF66      0x44 /* 0xFF33FF66 */
#define LCD_COLOR_CLUT8_RGB_33FF99      0x45 /* 0xFF33FF99 */
#define LCD_COLOR_CLUT8_RGB_33FFCC      0x46 /* 0xFF33FFCC */
#define LCD_COLOR_CLUT8_RGB_33FFFF      0x47 /* 0xFF33FFFF */
#define LCD_COLOR_CLUT8_RGB_660000      0x48 /* 0xFF660000 */
#define LCD_COLOR_CLUT8_RGB_660033      0x49 /* 0xFF660033 */
#define LCD_COLOR_CLUT8_RGB_660066      0x4A /* 0xFF660066 */
#define LCD_COLOR_CLUT8_RGB_660099      0x4B /* 0xFF660099 */
#define LCD_COLOR_CLUT8_RGB_6600CC      0x4C /* 0xFF6600CC */
#define LCD_COLOR_CLUT8_RGB_6600FF      0x4D /* 0xFF6600FF */
#define LCD_COLOR_CLUT8_RGB_663300      0x4E /* 0xFF663300 */
#define LCD_COLOR_CLUT8_RGB_663333      0x4F /* 0xFF663333 */
#define LCD_COLOR_CLUT8_RGB_663366      0x50 /* 0xFF663366 */
#define LCD_COLOR_CLUT8_RGB_663399      0x51 /* 0xFF663399 */
#define LCD_COLOR_CLUT8_RGB_6633CC      0x52 /* 0xFF6633CC */
#define LCD_COLOR_CLUT8_RGB_6633FF      0x53 /* 0xFF6633FF */
#define LCD_COLOR_CLUT8_RGB_666600      0x54 /* 0xFF666600 */
#define LCD_COLOR_CLUT8_RGB_666633      0x55 /* 0xFF666633 */
#define LCD_COLOR_CLUT8_RGB_666666      0x56 /* 0xFF666666 */
#define LCD_COLOR_CLUT8_RGB_666699      0x57 /* 0xFF666699 */
#define LCD_COLOR_CLUT8_RGB_6666CC      0x58 /* 0xFF6666CC */
#define LCD_COLOR_CLUT8_RGB_6666FF      0x59 /* 0xFF6666FF */
#define LCD_COLOR_CLUT8_RGB_669900      0x5A /* 0xFF669900 */
#define LCD_COLOR_CLUT8_RGB_669933      0x5B /* 0xFF669933 */
#define LCD_COLOR_CLUT8_RGB_669966      0x5C /* 0xFF669966 */
#define LCD_COLOR_CLUT8_RGB_669999      0x5D /* 0xFF669999 */
#define LCD_COLOR_CLUT8_RGB_6699CC      0x5E /* 0xFF6699CC */
#define LCD_COLOR_CLUT8_RGB_6699FF      0x5F /* 0xFF6699FF */
#define LCD_COLOR_CLUT8_RGB_66CC00      0x60 /* 0xFF66CC00 */
#define LCD_COLOR_CLUT8_RGB_66CC33      0x61 /* 0xFF66CC33 */
#define LCD_COLOR_CLUT8_RGB_66CC66      0x62 /* 0xFF66CC66 */
#define LCD_COLOR_CLUT8_RGB_66CC99      0x63 /* 0xFF66CC99 */
#define LCD_COLOR_CLUT8_RGB_66CCCC      0x64 /* 0xFF66CCCC */
#define LCD_COLOR_CLUT8_RGB_66CCFF      0x65 /* 0xFF66CCFF */
#define LCD_COLOR_CLUT8_RGB_66FF00      0x66 /* 0xFF66FF00 */
#define LCD_COLOR_CLUT8_RGB_66FF33      0x67 /* 0xFF66FF33 */
#define LCD_COLOR_CLUT8_RGB_66FF66      0x68 /* 0xFF66FF66 */
#define LCD_COLOR_CLUT8_RGB_66FF99      0x69 /* 0xFF66FF99 */
#define LCD_COLOR_CLUT8_RGB_66FFCC      0x6A /* 0xFF66FFCC */
#define LCD_COLOR_CLUT8_RGB_66FFFF      0x6B /* 0xFF66FFFF */
#define LCD_COLOR_CLUT8_RGB_990000      0x6C /* 0xFF990000 */
#define LCD_COLOR_CLUT8_RGB_990033      0x6D /* 0xFF990033 */
#define LCD_COLOR_CLUT8_RGB_990066      0x6E /* 0xFF990066 */
#define LCD_COLOR_CLUT8_RGB_990099      0x6F /* 0xFF990099 */
#define LCD_COLOR_CLUT8_RGB_9900CC      0x70 /* 0xFF9900CC */
#define LCD_COLOR_CLUT8_RGB_993300      0x72 /* 0xFF993300 */
#define LCD_COLOR_CLUT8_RGB_993333      0x73 /* 0xFF993333 */
#define LCD_COLOR_CLUT8_RGB_993366      0x74 /* 0xFF993366 */
#define LCD_COLOR_CLUT8_RGB_993399      0x75 /* 0xFF993399 */
#define LCD_COLOR_CLUT8_RGB_9933CC      0x76 /* 0xFF9933CC */
#define LCD_COLOR_CLUT8_RGB_9933FF      0x77 /* 0xFF9933FF */
#define LCD_COLOR_CLUT8_RGB_996600      0x78 /* 0xFF996600 */
#define LCD_COLOR_CLUT8_RGB_996633      0x79 /* 0xFF996633 */
#define LCD_COLOR_CLUT8_RGB_996666      0x7A /* 0xFF996666 */
#define LCD_COLOR_CLUT8_RGB_996699      0x7B /* 0xFF996699 */
#define LCD_COLOR_CLUT8_RGB_9966CC      0x7C /* 0xFF9966CC */
#define LCD_COLOR_CLUT8_RGB_9966FF      0x7D /* 0xFF9966FF */
#define LCD_COLOR_CLUT8_RGB_999900      0x7E /* 0xFF999900 */
#define LCD_COLOR_CLUT8_RGB_999933      0x7F /* 0xFF999933 */
#define LCD_COLOR_CLUT8_RGB_999966      0x80 /* 0xFF999966 */
#define LCD_COLOR_CLUT8_RGB_999999      0x81 /* 0xFF999999 */
#define LCD_COLOR_CLUT8_RGB_9999CC      0x82 /* 0xFF9999CC */
#define LCD_COLOR_CLUT8_RGB_9999FF      0x83 /* 0xFF9999FF */
#define LCD_COLOR_CLUT8_RGB_99CC00      0x84 /* 0xFF99CC00 */
#define LCD_COLOR_CLUT8_RGB_99CC33      0x85 /* 0xFF99CC33 */
#define LCD_COLOR_CLUT8_RGB_99CC66      0x86 /* 0xFF99CC66 */
#define LCD_COLOR_CLUT8_RGB_99CC99      0x87 /* 0xFF99CC99 */
#define LCD_COLOR_CLUT8_RGB_99CCCC      0x88 /* 0xFF99CCCC */
#define LCD_COLOR_CLUT8_RGB_99CCFF      0x89 /* 0xFF99CCFF */
#define LCD_COLOR_CLUT8_RGB_99FF00      0x8A /* 0xFF99FF00 */
#define LCD_COLOR_CLUT8_RGB_99FF33      0x8B /* 0xFF99FF33 */
#define LCD_COLOR_CLUT8_RGB_99FF66      0x8C /* 0xFF99FF66 */
#define LCD_COLOR_CLUT8_RGB_99FF99      0x8D /* 0xFF99FF99 */
#define LCD_COLOR_CLUT8_RGB_99FFCC      0x8E /* 0xFF99FFCC */
#define LCD_COLOR_CLUT8_RGB_99FFFF      0x8F /* 0xFF99FFFF */
#define LCD_COLOR_CLUT8_RGB_CC0000      0x90 /* 0xFFCC0000 */
#define LCD_COLOR_CLUT8_RGB_CC0033      0x91 /* 0xFFCC0033 */
#define LCD_COLOR_CLUT8_RGB_CC0066      0x92 /* 0xFFCC0066 */
#define LCD_COLOR_CLUT8_RGB_CC0099      0x93 /* 0xFFCC0099 */
#define LCD_COLOR_CLUT8_RGB_CC00CC      0x94 /* 0xFFCC00CC */
#define LCD_COLOR_CLUT8_RGB_CC00FF      0x95 /* 0xFFCC00FF */
#define LCD_COLOR_CLUT8_RGB_CC3300      0x96 /* 0xFFCC3300 */
#define LCD_COLOR_CLUT8_RGB_CC3333      0x97 /* 0xFFCC3333 */
#define LCD_COLOR_CLUT8_RGB_CC3366      0x98 /* 0xFFCC3366 */
#define LCD_COLOR_CLUT8_RGB_CC3399      0x99 /* 0xFFCC3399 */
#define LCD_COLOR_CLUT8_RGB_CC33CC      0x9A /* 0xFFCC33CC */
#define LCD_COLOR_CLUT8_RGB_CC33FF      0x9B /* 0xFFCC33FF */
#define LCD_COLOR_CLUT8_RGB_CC6600      0x9C /* 0xFFCC6600 */
#define LCD_COLOR_CLUT8_RGB_CC6633      0x9D /* 0xFFCC6633 */
#define LCD_COLOR_CLUT8_RGB_CC6666      0x9E /* 0xFFCC6666 */
#define LCD_COLOR_CLUT8_RGB_CC6699      0x9F /* 0xFFCC6699 */
#define LCD_COLOR_CLUT8_RGB_CC66CC      0xA0 /* 0xFFCC66CC */
#define LCD_COLOR_CLUT8_RGB_CC66FF      0xA1 /* 0xFFCC66FF */
#define LCD_COLOR_CLUT8_RGB_CC9900      0xA2 /* 0xFFCC9900 */
#define LCD_COLOR_CLUT8_RGB_CC9933      0xA3 /* 0xFFCC9933 */
#define LCD_COLOR_CLUT8_RGB_CC9966      0xA4 /* 0xFFCC9966 */
#define LCD_COLOR_CLUT8_RGB_CC9999      0xA5 /* 0xFFCC9999 */
#define LCD_COLOR_CLUT8_RGB_CC99CC      0xA6 /* 0xFFCC99CC */
#define LCD_COLOR_CLUT8_RGB_CC99FF      0xA7 /* 0xFFCC99FF */
#define LCD_COLOR_CLUT8_RGB_CCCC00      0xA8 /* 0xFFCCCC00 */
#define LCD_COLOR_CLUT8_RGB_CCCC33      0xA9 /* 0xFFCCCC33 */
#define LCD_COLOR_CLUT8_RGB_CCCC66      0xAA /* 0xFFCCCC66 */
#define LCD_COLOR_CLUT8_RGB_CCCC99      0xAB /* 0xFFCCCC99 */
#define LCD_COLOR_CLUT8_RGB_CCCCCC      0xAC /* 0xFFCCCCCC */
#define LCD_COLOR_CLUT8_RGB_CCCCFF      0xAD /* 0xFFCCCCFF */
#define LCD_COLOR_CLUT8_RGB_CCFF00      0xAE /* 0xFFCCFF00 */
#define LCD_COLOR_CLUT8_RGB_CCFF33      0xAF /* 0xFFCCFF33 */
#define LCD_COLOR_CLUT8_RGB_CCFF66      0xB0 /* 0xFFCCFF66 */
#define LCD_COLOR_CLUT8_RGB_CCFF99      0xB1 /* 0xFFCCFF99 */
#define LCD_COLOR_CLUT8_RGB_CCFFCC      0xB2 /* 0xFFCCFFCC */
#define LCD_COLOR_CLUT8_RGB_CCFFFF      0xB3 /* 0xFFCCFFFF */
#define LCD_COLOR_CLUT8_RGB_FF0033      0xB5 /* 0xFFFF0033 */
#define LCD_COLOR_CLUT8_RGB_FF0066      0xB6 /* 0xFFFF0066 */
#define LCD_COLOR_CLUT8_RGB_FF0099      0xB7 /* 0xFFFF0099 */
#define LCD_COLOR_CLUT8_RGB_FF00CC      0xB8 /* 0xFFFF00CC */
#define LCD_COLOR_CLUT8_RGB_FF3300      0xBA /* 0xFFFF3300 */
#define LCD_COLOR_CLUT8_RGB_FF3333      0xBB /* 0xFFFF3333 */
#define LCD_COLOR_CLUT8_RGB_FF3366      0xBC /* 0xFFFF3366 */
#define LCD_COLOR_CLUT8_RGB_FF3399      0xBD /* 0xFFFF3399 */
#define LCD_COLOR_CLUT8_RGB_FF33CC      0xBE /* 0xFFFF33CC */
#define LCD_COLOR_CLUT8_RGB_FF33FF      0xBF /* 0xFFFF33FF */
#define LCD_COLOR_CLUT8_RGB_FF6633      0xC1 /* 0xFFFF6633 */
#define LCD_COLOR_CLUT8_RGB_FF6666      0xC2 /* 0xFFFF6666 */
#define LCD_COLOR_CLUT8_RGB_FF6699      0xC3 /* 0xFFFF6699 */
#define LCD_COLOR_CLUT8_RGB_FF66CC      0xC4 /* 0xFFFF66CC */
#define LCD_COLOR_CLUT8_RGB_FF66FF      0xC5 /* 0xFFFF66FF */
#define LCD_COLOR_CLUT8_RGB_FF9900      0xC6 /* 0xFFFF9900 */
#define LCD_COLOR_CLUT8_RGB_FF9933      0xC7 /* 0xFFFF9933 */
#define LCD_COLOR_CLUT8_RGB_FF9966      0xC8 /* 0xFFFF9966 */
#define LCD_COLOR_CLUT8_RGB_FF9999      0xC9 /* 0xFFFF9999 */
#define LCD_COLOR_CLUT8_RGB_FF99CC      0xCA /* 0xFFFF99CC */
#define LCD_COLOR_CLUT8_RGB_FF99FF      0xCB /* 0xFFFF99FF */
#define LCD_COLOR_CLUT8_RGB_FFCC00      0xCC /* 0xFFFFCC00 */
#define LCD_COLOR_CLUT8_RGB_FFCC33      0xCD /* 0xFFFFCC33 */
#define LCD_COLOR_CLUT8_RGB_FFCC66      0xCE /* 0xFFFFCC66 */
#define LCD_COLOR_CLUT8_RGB_FFCC99      0xCF /* 0xFFFFCC99 */
#define LCD_COLOR_CLUT8_RGB_FFCCCC      0xD0 /* 0xFFFFCCCC */
#define LCD_COLOR_CLUT8_RGB_FFCCFF      0xD1 /* 0xFFFFCCFF */
#define LCD_COLOR_CLUT8_RGB_FFFF33      0xD3 /* 0xFFFFFF33 */
#define LCD_COLOR_CLUT8_RGB_FFFF66      0xD4 /* 0xFFFFFF66 */
#define LCD_COLOR_CLUT8_RGB_FFFF99      0xD5 /* 0xFFFFFF99 */
#define LCD_COLOR_CLUT8_RGB_FFFFCC      0xD6 /* 0xFFFFFFCC */
#define LCD_COLOR_CLUT8_WHITE           0xD7 /* 0xFFFFFFFF */
#define LCD_COLOR_CLUT8_YELLOW          0xD2 /* 0xFFFFFF00 */

#define LCD_COLOR_RGB565_BLACK			0x0000
#define LCD_COLOR_RGB565_BLUE			0x001F
#define LCD_COLOR_RGB565_GREEN			0x07E0
#define LCD_COLOR_RGB565_GREY			0x8410
#define LCD_COLOR_RGB565_LIGHT_BLUE		0x07FF
#define LCD_COLOR_RGB565_ORANGE			0xFB20
#define LCD_COLOR_RGB565_PURPLE			0x981F
#define LCD_COLOR_RGB565_RED			0xF800
#define LCD_COLOR_RGB565_WHITE			0xFFFF
#define LCD_COLOR_RGB565_YELLOW			0xFFE0

#define LCD_COLOR_RGB888_BLACK			0x000000
#define LCD_COLOR_RGB888_BLUE			0x0000FF
#define LCD_COLOR_RGB888_GREEN			0x00FF00
#define LCD_COLOR_RGB888_GREY			0x808080
#define LCD_COLOR_RGB888_LIGHT_BLUE		0x00FFFF
#define LCD_COLOR_RGB888_ORANGE			0xFF6600
#define LCD_COLOR_RGB888_PURPLE			0x9900FF
#define LCD_COLOR_RGB888_RED			0xFF0000
#define LCD_COLOR_RGB888_WHITE			0xFFFFFF
#define LCD_COLOR_RGB888_YELLOW			0xFFFF00

#define LCD_VLineType                   LCD_HLineType

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LCD_COLOR_FORMAT_ARGB1555,
    LCD_COLOR_FORMAT_ARGB4444,
    LCD_COLOR_FORMAT_ARGB8888,
	LCD_COLOR_FORMAT_CLUT8,
    LCD_COLOR_FORMAT_RGB565,
    LCD_COLOR_FORMAT_RGB666,
    LCD_COLOR_FORMAT_RGB888
} LCD_ColorFormatEnum;

typedef enum {
#if LCD_EN_EK79001
    LCD_MODEL_EK79001,
#endif
#if LCD_EN_EK79007AD2
    LCD_MODEL_EK79007AD2,
#endif
#if LCD_EN_ILI9881C
    LCD_MODEL_ILI9881C,
#endif
#if LCD_EN_JD9365HD
    LCD_MODEL_JD9365HD,
#endif
#if LCD_EN_ST7796U
    LCD_MODEL_ST7796U,
#endif
#if LCD_EN_TFP410PAPR
    LCD_MODEL_TFP410PAPR,
#endif
    LCD_MODEL_UNKNOW
} LCD_ModelEnum;

typedef enum {
    LCD_ORIENTATION_HORIZONTAL,
    LCD_ORIENTATION_HORIZONTAL_FLIP,
    LCD_ORIENTATION_VERTICAL,
    LCD_ORIENTATION_VERTICAL_FLIP
} LCD_OrientationEnum;

typedef enum {
	LCD_PERI_UNUSED = 0,
    LCD_PERI_GLCDC,
    LCD_PERI_I2C,
    LCD_PERI_MIPI,
    LCD_PERI_SCI_SPI,
    LCD_PERI_SPI
} LCD_PeriModelEnum;

typedef struct {
    LCD_ModelEnum model;
    const void *peri_1;
    const void *peri_2;
    LCD_PeriModelEnum peri_model_1;
    LCD_PeriModelEnum peri_model_2;
#if LCD_EN_PERI_I2C
    /* 同步 IIC 回调：封装从机地址，返回 FSP 错误码；iicRead 先写寄存器地址再读取。 */
    uint32_t (*iicWrite)(uint8_t *data, uint8_t len);
    uint32_t (*iicRead)(uint8_t *data_w, uint8_t len_w, uint8_t *data_r, uint8_t len_r);
#endif
} LCD_ConfigType;

typedef struct {
	const uint8_t *src;
	LCD_ColorFormatEnum src_format;
	uint32_t total_pixels;
	uint8_t *out;
	LCD_ColorFormatEnum out_format;
	int alpha;
} LCD_ColorConvertType;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t length;
    uint16_t width;
    bool valid;
} LCD_DirtyRectType;

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t length;
	uint16_t width;
	const uint8_t *bitmap;
} LCD_BitmapType;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t length;
    uint32_t color;
} LCD_HLineType;

typedef struct {
    uint16_t x_start;
    uint16_t y_start;
    uint16_t x_end;
    uint16_t y_end;
    uint32_t color;
} LCD_LineType;

typedef struct {
	uint16_t x;             /* 外框左上角 X */
	uint16_t y;             /* 外框左上角 Y */
	uint16_t length;        /* 外框水平像素数 */
	uint16_t width;         /* 外框垂直像素数 */
	uint16_t border_width;  /* 四条边向内延伸的厚度，至少为 1 */
	uint32_t color;         /* 边框颜色 */
} LCD_OutlineRectangleType;

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t length;
	uint16_t width;
	uint32_t color;
} LCD_RectangelType;

typedef struct {
    uint8_t index;
	uint16_t x_length;
	uint16_t x_dummy;
	uint16_t y_length;
	uint16_t y_dummy;
	uint8_t bytes_per_pixel;
	LCD_ColorFormatEnum color_format;

	uint8_t *graphic_mem_1;
    bool mem1_read_only;
	uint8_t *graphic_mem_2;
    bool mem2_read_only;
	uint8_t *graphic_writeable;

    /* GLCDC Layer 配置，实际指向 display_runtime_cfg_t * 类型
     * 各屏幕模块的初始化函数中自行分配空间，若不使用，保留为 NULL */
    void *runtime_cfg;
	uint8_t * volatile graphic_displayed;	/* 当前已确认生效的显示缓冲，独立于 graphic_mem_1/2 槽位 */
    uint8_t *graphic_to_present;			/* 已登记但尚未提交的显示目标，可指向只读缓冲；NULL 表示没有待显示目标 */
    uint8_t * volatile graphic_submitted;	/* 已提交、等待硬件地址切换完成的缓冲；NULL 表示没有挂起提交 */
    LCD_DirtyRectType dirty_current;	    /* 当前可写帧缓冲已修改区域的包围矩形 */
    LCD_DirtyRectType dirty_submitted;	    /* 上一已提交帧需要同步到新可写帧缓冲的包围矩形 */
    volatile bool present_pending;		    /* 仅表示硬件提交尚未完成，登记待显示目标时不置位 */
} LCD_LayerType;

typedef struct LCD_DeviceType {
    /* 初始化时从 LCD_ConfigType 复制 */
    LCD_ModelEnum model;
    const void *peri_1;
    const void *peri_2;
    LCD_PeriModelEnum peri_model_1;
    LCD_PeriModelEnum peri_model_2;

    const char *name;
    volatile uint32_t vsync_cnt;
    LCD_OrientationEnum orientation;
    LCD_LayerType layer_1;
    LCD_LayerType layer_2;

#if LCD_EN_PERI_GLCDC
    volatile uint32_t gr1_underflow_cnt;
    volatile uint32_t gr2_underflow_cnt;
#endif

#if LCD_EN_PERI_MIPI
    volatile uint32_t mipi_timing_err_cnt;
    volatile uint32_t mipi_underflow_cnt;
    volatile uint32_t mipi_overflow_cnt;
#endif

    uint32_t (*drawBitmap)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_BitmapType *bitmap);
    uint32_t (*drawHLine)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_HLineType *hline);
    uint32_t (*drawLine)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_LineType *line);
    uint32_t (*drawOutlineRectangle)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_OutlineRectangleType *rectangle);
    uint32_t (*drawPoint)(struct LCD_DeviceType *lcd, uint8_t layer, uint16_t x, uint16_t y, uint32_t color);
    uint32_t (*drawVLine)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_VLineType *vline);
    uint32_t (*fill)(struct LCD_DeviceType *lcd, uint8_t layer, uint32_t color);
    uint32_t (*fillRectangle)(struct LCD_DeviceType *lcd, uint8_t layer, LCD_RectangelType *rectangle);
    uint32_t (*init)(struct LCD_DeviceType *lcd);
    uint32_t (*isIdle)(struct LCD_DeviceType *lcd, uint8_t layer, bool *idle);
    uint32_t (*moveLayer)(struct LCD_DeviceType *lcd, uint8_t layer, int16_t x, int16_t y);
    uint32_t (*present)(struct LCD_DeviceType *lcd, uint8_t layer);
    uint32_t (*setBackLight)(struct LCD_DeviceType *lcd, uint8_t percent);
    uint32_t (*setDefaultCLUT)(struct LCD_DeviceType *lcd, uint8_t layer);
    uint32_t (*updateLayer1Mem)(struct LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show);
    uint32_t (*updateLayer2Mem)(struct LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show);

#if LCD_EN_PERI_I2C
    uint32_t (*iicWrite)(uint8_t *data, uint8_t len);
    uint32_t (*iicRead)(uint8_t *data_w, uint8_t len_w, uint8_t *data_r, uint8_t len_r);
#endif
} LCD_DeviceType;

uint32_t LCD_ConvetColor(LCD_ColorConvertType *cct);
uint32_t LCD_DrawBitmap(LCD_DeviceType *lcd, uint8_t layer, LCD_BitmapType *bitmap);
uint32_t LCD_DrawHLine(LCD_DeviceType *lcd, uint8_t layer, LCD_HLineType *hline);
uint32_t LCD_DrawLine(LCD_DeviceType *lcd, uint8_t layer, LCD_LineType *line);
uint32_t LCD_DrawOutlineRectangle(LCD_DeviceType *lcd, uint8_t layer, LCD_OutlineRectangleType *rectangle);
uint32_t LCD_DrawPoint(LCD_DeviceType *lcd, uint8_t layer, uint16_t x, uint16_t y, uint32_t color);
uint32_t LCD_DrawVLine(LCD_DeviceType *lcd, uint8_t layer, LCD_VLineType *vline);
uint32_t LCD_Fill(LCD_DeviceType *lcd, uint8_t layer, uint32_t color);
uint32_t LCD_FillRectangle(LCD_DeviceType *lcd, uint8_t layer, LCD_RectangelType *rectangle);
uint32_t LCD_Initialize(LCD_DeviceType *lcd, LCD_ConfigType *cfg);
uint32_t LCD_IsIdle(LCD_DeviceType *lcd, uint8_t layer, bool *idle);
uint32_t LCD_MoveLayer(LCD_DeviceType *lcd, uint8_t layer, int16_t x, int16_t y);
uint32_t LCD_Present(LCD_DeviceType *lcd, uint8_t layer);
uint32_t LCD_SetBackLight(LCD_DeviceType *lcd, uint8_t percent);
uint32_t LCD_SetDefaultCLUT(LCD_DeviceType *lcd, uint8_t layer);
uint32_t LCD_UpdateLayer1Mem(LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show);
uint32_t LCD_UpdateLayer2Mem(LCD_DeviceType *lcd, uint8_t mem_num, uint8_t *new_buffer, bool read_only, bool immediate_show);

#ifdef __cplusplus
}
#endif

#endif
