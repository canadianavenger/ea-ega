/*
 * ega2bmp.c 
 * Converts a given EA-EGA image to a Windows BMP image
 *  
 * This code is offered without warranty under the MIT License. Use it as you will 
 * personally or commercially, just give credit if you do.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "bmp.h"

#define OUTEXT ".BMP"

typedef struct {
    size_t      len;         // length of buffer in bytes
    size_t      pos;         // current byte position in buffer
    uint8_t     *data;       // pointer to bytes in memory
} memstream_buf_t;

// default EGA/VGA 16 colour palette
static bmp_palette_entry_t ega_pal[16] = { 
  {0x00,0x00,0x00,0x00}, {0xaa,0x00,0x00,0x00}, {0x00,0xaa,0x00,0x00}, {0xaa,0xaa,0x00,0x00}, 
  {0x00,0x00,0xaa,0x00}, {0xaa,0x00,0xaa,0x00}, {0x00,0x55,0xaa,0x00}, {0xaa,0xaa,0xaa,0x00},
  {0x55,0x55,0x55,0x00}, {0xff,0x55,0x55,0x00}, {0x55,0xff,0x55,0x00}, {0xff,0xff,0x55,0x00}, 
  {0x55,0x55,0xff,0x00}, {0xff,0x55,0xff,0x00}, {0x55,0xff,0xff,0x00}, {0xff,0xff,0xff,0x00},
};

size_t filesize(FILE *f);
void drop_extension(char *fn);
char *filename(char *path);
#define fclose_s(A) if(A) fclose(A); A=NULL
#define free_s(A) if(A) free(A); A=NULL
int save_bmp(const char *fn, memstream_buf_t *src, uint16_t width, uint16_t height, bmp_palette_entry_t *pal);

int main(int argc, char *argv[]) {
    int rval = -1;
    FILE *fi = NULL;
    char *fi_name = NULL;
    char *fo_name = NULL;
    memstream_buf_t img = {0, 0, NULL}; // decoded image
    memstream_buf_t src = {0, 0, NULL}; // encoded image data
    uint16_t width = 0;
    uint16_t height = 0;

    printf("Electronic Arts EGA image format to BMP image converter\n");

    if((argc < 2) || (argc > 3)) {
        printf("USAGE: %s [infile] <outfile>\n", filename(argv[0]));
        printf("[infile] is the name of the input file\n");
        printf("<outfile> is optional and the name of the output file\n");
        printf("if omitted, outfile will be named the same as infile with a '%s' extension\n", OUTEXT);
        return -1;
    }
    argv++; argc--; // consume the first arg (program name)

    // get the filename strings from command line
    int namelen = strlen(argv[0]);
    if(NULL == (fi_name = calloc(1, namelen+1))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    strncpy(fi_name, argv[0], namelen);
    argv++; argc--; // consume the arg (input file)

    if(argc) { // output file name was provided
        int namelen = strlen(argv[0]);
        if(NULL == (fo_name = calloc(1, namelen+1))) {
            printf("Unable to allocate memory\n");
            goto CLEANUP;
        }
        strncpy(fo_name, argv[0], namelen);
        argv++; argc--; // consume the arg (input file)
    } else { // no name was provded, so make one
        if(NULL == (fo_name = calloc(1, namelen+5))) {
            printf("Unable to allocate memory\n");
            goto CLEANUP;
        }
        strncpy(fo_name, fi_name, namelen);
        drop_extension(fo_name); // remove exisiting extension
        strncat(fo_name, OUTEXT, namelen+4); // add bmp extension
    }

    // open the input file
    printf("Opening EGA File: '%s'", fi_name);
    if(NULL == (fi = fopen(fi_name,"rb"))) {
        printf("Error: Unable to open input file\n");
        goto CLEANUP;
    }

    // determine size of image file
    size_t fsz = filesize(fi);
    printf("\tFile Size: %zu\n", fsz);

    // allocate the packed image buffer based on the file size
    if(NULL == (src.data = calloc(1, fsz))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }
    src.len = fsz;

    // read in the file
    int nr = fread(src.data, src.len, 1, fi);
    if(1 != nr) {
        printf("Error Unable read input file\n");
        goto CLEANUP;
    }

    int16_t *par = (int16_t *)src.data;
    width = *par++;
    height = *par;
    width++;
    height++;
    src.pos += 2 * sizeof(int16_t);

    printf("Resolution: %d x %d\n", width, height);

    img.len = width * height;

    if(NULL == (img.data = calloc(1, img.len))) {
        printf("Error: Unable to allocate buffer for output image\n");
        goto CLEANUP;
    }

    // image is stored from bottom scanline to top
    // so start by pointing to the beginning of the last line.
    uint8_t *dst = &img.data[(height-1) * width];
    int x = 0;

    // fortunately the image data is compressed on a per line
    // basis, so we don't need to worry about the runs spanning
    // a line boundary
    while(src.pos < src.len) {
        uint8_t tc = src.data[src.pos++];
        if(tc >= 128) {
            tc = (tc & 0x7f) + 3;
            uint8_t tv = src.data[src.pos++];
printf("%d [%02x]\n", tc, tv);
            for(int i = 0; i< tc; i++) {
                *dst++ = (tv >> 4) & 0x0f;
                *dst++ = tv & 0x0f;
                x += 2;
            }
        } else {
            tc += 1;
printf("%d [", tc);
            for(int i = 0; i< tc; i++) {
                uint8_t tv = src.data[src.pos++];
printf(" %02x", tv);
                *dst++ = (tv >> 4) & 0x0f;
                *dst++ = tv & 0x0f;
                x += 2;
            }
printf(" ]\n");
        }
        if(x >= width) {
            x = 0;
            dst -= (2 * width); // move back to the start of the previous line
        }
    }

    if(save_bmp(fo_name, &img, width, height, ega_pal)) {
            printf("Unable to write BMP image\n");
            goto CLEANUP;
    }

    printf("Done\n");
    rval = 0; // clean exit
CLEANUP:
    fclose_s(fi);
    free_s(fi_name);
    free_s(fo_name);
    free_s(img.data);
    free_s(src.data);
    return rval;
}

/// @brief determins the size of the file
/// @param f handle to an open file
/// @return returns the size of the file
size_t filesize(FILE *f) {
    size_t szll, cp;
    cp = ftell(f);           // save current position
    fseek(f, 0, SEEK_END);   // find the end
    szll = ftell(f);         // get positon of the end
    fseek(f, cp, SEEK_SET);  // restore the file position
    return szll;             // return position of the end as size
}

/// @brief removes the extension from a filename
/// @param fn sting pointer to the filename
void drop_extension(char *fn) {
    char *extension = strrchr(fn, '.');
    if(NULL != extension) *extension = 0; // strip out the existing extension
}

/// @brief Returns the filename portion of a path
/// @param path filepath string
/// @return a pointer to the filename portion of the path string
char *filename(char *path) {
	int i;

	if(path == NULL || path[0] == '\0')
		return "";
	for(i = strlen(path) - 1; i >= 0 && path[i] != '/'; i--);
	if(i == -1)
		return "";
	return &path[i+1];
}

// allocate a header buffer large enough for all 3 parts, plus 16 bit padding at the start to 
// maintian 32 bit alignment after the 16 bit signature.
#define HDRBUFSZ (sizeof(bmp_signature_t) + sizeof(bmp_header_t))

/// @brief saves the image pointed to by src as a BMP, assumes 16 colour 1 byte per pixel image data
/// @param fn name of the file to create and write to
/// @param src memstream buffer pointer to the source image data
/// @param width  width of the image in pixels
/// @param height height of the image in pixels or lines
/// @param pal pointe to 16 entry palette
/// @return 0 on success, otherwise an error code
int save_bmp(const char *fn, memstream_buf_t *src, uint16_t width, uint16_t height, bmp_palette_entry_t *pal) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer, also holds header info

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == src) || (NULL == src->data)) {
        rval = -1;  // NULL pointer error
        goto bmp_cleanup;
    }

    // try to open/create output file
    if(NULL == (fp = fopen(fn,"wb"))) {
        rval = -2;  // can't open/create output file
        goto bmp_cleanup;
    }

    // stride is the bytes per line in the BMP file, which are padded
    // out to 32 bit boundaries
    uint32_t stride = ((width + 3) & (~0x0003)) / 2; // we get 2 pixels per byte for being 16 colour
    uint32_t bmp_img_sz = (stride) * height;

    // allocate a buffer to hold the header and a single scanline of data
    // this could be optimized if necessary to only allocate the larger of
    // the line buffer, or the header + padding as they are used at mutually
    // exclusive times
    if(NULL == (buf = calloc(1, HDRBUFSZ + stride + 2))) {
        rval = -3;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // signature starts after padding to maintain 32bit alignment for the rest of the header
    bmp_signature_t *sig = (bmp_signature_t *)&buf[stride + 2];

    // bmp header starts after signature
    bmp_header_t *bmp = (bmp_header_t *)&buf[stride + 2 + sizeof(bmp_signature_t)];

    // setup the signature and DIB header fields
    *sig = BMPFILESIG;
    size_t palsz = sizeof(bmp_palette_entry_t) * 16;
    bmp->dib.image_offset = HDRBUFSZ + palsz;
    bmp->dib.file_size = bmp->dib.image_offset + bmp_img_sz;

    // setup the bmi header fields
    bmp->bmi.header_size = sizeof(bmi_header_t);
    bmp->bmi.image_width = width;
    bmp->bmi.image_height = height;
    bmp->bmi.num_planes = 1;           // always 1
    bmp->bmi.bits_per_pixel = 4;       // 16 colour image
    bmp->bmi.compression = 0;          // uncompressed
    bmp->bmi.bitmap_size = bmp_img_sz;
    bmp->bmi.horiz_res = BMP96DPI;
    bmp->bmi.vert_res = BMP96DPI;
    bmp->bmi.num_colors = 16;          // palette has 16 colours
    bmp->bmi.important_colors = 0;     // all colours are important

    // write out the header
    int nr = fwrite(sig, HDRBUFSZ, 1, fp);
    if(1 != nr) {
        rval = -4;  // unable to write file
        goto bmp_cleanup;
    }

    // we're using our global palette here, wich is already in BMP format
    // write out the palette
    nr = fwrite(pal, palsz, 1, fp);
    if(1 != nr) {
        rval = -4;  // can't write file
        goto bmp_cleanup;
    }

    // now we need to output the image scanlines. For maximum
    // compatibility we do so in the natural order for BMP
    // which is from bottom to top. For 16 colour/4 bit image
    // the pixels are packed two per byte, left most pixel in
    // the most significant nibble.
    // start by pointing to start of last line of data
    uint8_t *px = &src->data[src->len - width];
    // loop through the lines
    for(int y = 0; y < height; y++) {
        memset(buf, 0, stride); // zero out the line in the output buffer
        // loop through all the pixels for a line
        // we are packing 2 pixels per byte, so width is half
        for(int x = 0; x < ((width + 1) / 2); x++) {
            uint8_t sp = *px++;          // get the first pixel
            sp <<= 4;                    // shift to make room
            if((x * 2 + 1) < width) {    // test for odd pixel end
                sp |= (*px++) & 0x0f;    // get the next pixel
            }
            buf[x] = sp;                 // write it to the line buffer
        }
        nr = fwrite(buf, stride, 1, fp); // write out the line
        if(1 != nr) {
            rval = -4;  // unable to write file
            goto bmp_cleanup;
        }
        px -= (width * 2); // move back to start of previous line
    }

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    return rval;
}
