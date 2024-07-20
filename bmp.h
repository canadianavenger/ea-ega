/*
 * bmp.h 
 * structure definitions for a Windows BMP file
 * 
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */
#include <stdint.h>

#ifndef IMG_BMP
#define IMG_BMP

typedef struct {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a; // this is reserved and 0 for BMP
} bmp_palette_entry_t;

#define BMPFILESIG (0x4d42)  // "BM" // Windows BMP signature
typedef uint16_t bmp_signature_t;

typedef struct {
	uint32_t  file_size;     // File Size in bytes
	uint32_t  RES;           // reserved (always 0)
	uint32_t  image_offset;  // File offset to image raster data
} dib_header_t;

#define BMP72DPI (2835) // 72 DPI converted to PPM
#define BMP96DPI (3780) // 96 DPI converted to PPM
typedef struct {
	uint32_t  header_size;       // size of this header (40)
	uint32_t  image_width;       // bitmap width
	int32_t   image_height;      // bitmap height (can be -ive to flip scan order)
	uint16_t  num_planes;        // Number of planes (must be 1)
	uint16_t  bits_per_pixel;    // 1,4,8,18,24 (some versions support 2 and 32)
	uint32_t  compression;       // 0 = uncompressed
	uint32_t  bitmap_size;       // Size of image or can be left at 0
	uint32_t  horiz_res;         // horizontal Pixels per meter (PPM)
	uint32_t  vert_res;          // vertical pixels per meter (PPM)
	uint32_t  num_colors;        // number of colours in the palette
	uint32_t  important_colors;  // number of important colours (0 = all)
} bmi_header_t;

typedef struct {
    dib_header_t dib;
    bmi_header_t bmi;
} bmp_header_t;

#endif
