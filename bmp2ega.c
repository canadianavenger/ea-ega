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

#define OUTEXT ".EGA"

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
int load_bmp(memstream_buf_t *dst, const char *fn, uint16_t *width, uint16_t *height);
int find_run(uint8_t *buf, uint8_t len, int *pos);

int main(int argc, char *argv[]) {
    int rval = -1;
    FILE *fo = NULL;
    char *fi_name = NULL;
    char *fo_name = NULL;
    memstream_buf_t img = {0, 0, NULL}; // source image
    memstream_buf_t dst = {0, 0, NULL}; // encoded image data
    uint16_t width = 0;
    uint16_t height = 0;

    printf("BMP image to Electronic Arts EGA image format converter\n");

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

    if(load_bmp(&img, fi_name, &width, &height)) {
            printf("Unable to read BMP image\n");
            goto CLEANUP;
    }

    // Allocate a destination buffer, use same size as image, it should never be bigger
    // espcially since the data is packed 2 to a byte
    dst.len = width * height;
    if(NULL == (dst.data = calloc(dst.len, 1))) {
        printf("Unable to allocate memory\n");
        goto CLEANUP;
    }

    // line based RLE compression here, but stored bottom to top
    // first add our image size prefix to the output stream
    int16_t *par = (int16_t *)dst.data;
    *par++ = (width - 1);
    *par = (height - 1);
    dst.pos += 2 * sizeof(int16_t);

    // pack the source pixels
    uint8_t *sp, *dp;
    sp = img.data;
    dp = img.data;
    for(size_t i = 0; i < img.len; i+=2) {
        uint8_t px = *sp++;
        px <<= 4;
        px |= ((*sp++) & 0x0f);
        *dp++ = px;
    }
    // our width is 1/2 because we are packed 2 pixels per byte now
    width /= 2; 

    // image is stored from bottom scanline to top
    // so start by pointing to the beginning of the last line.
    uint8_t *src = &img.data[(height-1) * width];

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width;) {
            int rpos = 0;
            int len = find_run(src, width - x, &rpos);

            if(rpos) { // we have bytes to copy before the found run (or we have no run)
                int clen = rpos;
                while(clen > 128) { // we have a run longer than the maximal encode length
                    dst.data[dst.pos++] = 127;     // max copy length (encoded as len-1)
                    for(int i = 0; i < 128; i++) { // copy the data
                        dst.data[dst.pos++] = *src++;
                    }
                    clen -= 128;
                }
                dst.data[dst.pos++] = clen - 1; // copy length (encoded as len-1)
                for(int i = 0; i < clen; i++) { // copy the data
                    dst.data[dst.pos++] = *src++;
                }
                x += rpos; // adjust our position in the line
            }
            if(len) { // we found a run-length to encode
                int rlen = len;
                while(rlen > 130) {
                    dst.data[dst.pos++] = 127 + 0x80; // max run length (encoded as len-3) + flag
                    dst.data[dst.pos++] = *src;       // value of byte to be replicated
                    rlen -= 130;
                }
                dst.data[dst.pos++] = (rlen - 3) + 0x80; // run length (encoded as len-3) + flag
                dst.data[dst.pos++] = *src;              // value of byte to be replicated
                x += len;   // adjust our position in the line
                src += len; // advance our pointer as well
            }
        }
        src -= (2 * width); // advance to the start of the previous scanline
    }

    // create/open the output file
    printf("Creating EGA File: '%s'\n", fo_name);
    if(NULL == (fo = fopen(fo_name,"wb"))) {
        printf("Error: Unable to open output file\n");
        goto CLEANUP;
    }
    int nw = fwrite(dst.data, dst.pos, 1, fo);
    if(1 != nw) {
        printf("Error Unable write file\n");
        goto CLEANUP;
    }

    printf("Done\n");
    rval = 0; // clean exit
CLEANUP:
    fclose_s(fo);
    free_s(fi_name);
    free_s(fo_name);
    free_s(img.data);
    free_s(dst.data);
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

/// @brief find the next run in the buffer passed in
/// @param buf pointer to the data
/// @param len length of the data
/// @param rpos index of the beginning of the run in the data
/// @return  length of the run found
int find_run(uint8_t *buf, uint8_t len, int *rpos) {
    uint8_t lc = *buf++; // last char to compare to
    int lp = 0;          // position of "last char"
    int count = 1;       // lenght of run
    int pos = 1;         // current position in buffer
    while(pos < len) {
        uint8_t cc = *buf++; // current char for comparison
        // run has ended (or hasn't started yet)
        if(lc != cc) {
            // valid runs are >= 3
            if(3 <= count) {
                *rpos = lp;   // return the start position of the run
                return count; // return the length of the run
            }
            // wasn't a valid run, so restart from current position
            lp = pos;
            count = 1;
            lc = cc;
        } else { // in a run
            count++; // count it
        }
        pos++; // advance
    }
    // return our stop position if we didn't find any runs >= 3
    if(3 > count) {
        *rpos = pos; // return our end position
        return 0;    // return a length of 0 as we don't have a valid run
    }
    // return last start position
    *rpos = lp;   // return the start position of the run
    return count; // return the length of the run
}



/// @brief loads the BMP image from a file, assumes 16 colour image. palette is ignored, assumed to follow 
///        CGA/EGA/VGA standard palette
/// @param dst pointer to a empty memstream buffer struct. load_bmp will allocate the buffer, image will be stored as 1 byte per pixel
/// @param fn name of file to load
/// @param width  pointer to width of the image in pixels set on return
/// @param height pointer to height of the image in pixels or lines set on return
/// @return  0 on success, otherwise an error code
int load_bmp(memstream_buf_t *dst, const char *fn, uint16_t *width, uint16_t *height) {
    int rval = 0;
    FILE *fp = NULL;
    uint8_t *buf = NULL; // line buffer
    bmp_header_t *bmp = NULL;

    // do some basic error checking on the inputs
    if((NULL == fn) || (NULL == dst) || (NULL == width) || (NULL == height)) {
        rval = -1;  // NULL pointer error
        goto bmp_cleanup;
    }

    // try to open input file
    if(NULL == (fp = fopen(fn,"rb"))) {
        rval = -2;  // can't open input file
        goto bmp_cleanup;
    }

    bmp_signature_t sig = 0;
    int nr = fread(&sig, sizeof(bmp_signature_t), 1, fp);
    if(1 != nr) {
        rval = -3;  // unable to read file
        goto bmp_cleanup;
    }
    if(BMPFILESIG != sig) {
        rval = -4; // not a BMP file
        goto bmp_cleanup;
    }

    // allocate a buffer to hold the header 
    if(NULL == (bmp = calloc(1, sizeof(bmp_header_t)))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }
    nr = fread(bmp, sizeof(bmp_header_t), 1, fp);
    if(1 != nr) {
        rval = -3;  // unable to read file
        goto bmp_cleanup;
    }

    // check some basic header vitals to make sure it's in a format we can work with
    if((1 != bmp->bmi.num_planes) || 
       (sizeof(bmi_header_t) != bmp->bmi.header_size) || 
       (0 != bmp->dib.RES)) {
        rval = -6;  // invalid header
        goto bmp_cleanup;
    }
    if((4 != bmp->bmi.bits_per_pixel) || 
       (16 != bmp->bmi.num_colors) || 
       (0 != bmp->bmi.compression)) {
        rval = -7;  // unsupported BMP format
        goto bmp_cleanup;
    }
    
    // seek to the start of the image data, as we don't use the palette data
    // we assume the standard CGA/EGA/VGA 16 colour palette
    fseek(fp, bmp->dib.image_offset, SEEK_SET);

    // check if the destination buffer is null, if not, free it
    // we will allocate it ourselves momentarily
    if(NULL != dst->data) {
        free(dst->data);
        dst->data = NULL;
    }

    // if height is negative, flip the render order
    bool flip = (bmp->bmi.image_height < 0); 
    bmp->bmi.image_height = abs(bmp->bmi.image_height);

    uint16_t lw = bmp->bmi.image_width;
    uint16_t lh = bmp->bmi.image_height;

    // stride is the bytes per line in the BMP file, which are padded
    // we get 2 pixels per byte for being 16 colour
    uint32_t stride = ((lw + 3) & (~0x0003)) / 2; 

    // allocate our line and output buffers
    if(NULL == (dst->data = calloc(1, lw * lh))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }
    dst->len = lw * lh;
    dst->pos = 0;

    if(NULL == (buf = calloc(1, stride))) {
        rval = -5;  // unable to allocate mem
        goto bmp_cleanup;
    }

    // now we need to read the image scanlines. 
    // start by pointing to start of last line of data
    uint8_t *px = &dst->data[dst->len - lw]; 
    if(flip) px = dst->data; // if flipped, start at beginning
    // loop through the lines
    for(int y = 0; y < lh; y++) {
        nr = fread(buf, stride, 1, fp); // read a line
        if(1 != nr) {
            rval = -3;  // unable to read file
            goto bmp_cleanup;
        }

        // loop through all the pixels for a line
        // we are packing 2 pixels per byte, so width is half
        for(int x = 0; x < ((lw + 1) / 2); x++) {
            uint8_t sp = buf[x];      // get the pixel pair
            *px++ = (sp >> 4) & 0x0f; // write the 1st pixel
            if((x * 2 + 1) < lw) {    // test for odd pixel end
                *px++ = sp & 0x0f;    // write the 2nd pixel
            }
        }
        if(!flip) { // if not flipped, wehave to walk backwards
            px -= (lw * 2); // move back to start of previous line
        }
    }

    *width = lw;
    *height = lh;

bmp_cleanup:
    fclose_s(fp);
    free_s(buf);
    free_s(bmp);
    return rval;
}
