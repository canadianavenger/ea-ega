# ea-ega
This repository is a companion to my blog post about the reverse engineering of the '.EGA' file format used with *688 Attack Sub*, and possibly others, by *Electronic Arts (EA)*. For more detail please read [my blog post](https://canadianavenger.io/2024/07/15/attack-of-the-subs/), and the followup for the [compression](https://canadianavenger.io/2024/07/20/running-on-empty/).

## EA Titles Known to Use the EGA Format
- 688 Attack Sub (1989)

## The Code

In this repo there are two C programs, each is a standalone utility for converting between the EA-PAK format and the Windows BMP format. The code is written to be portable, and should be able to be compiled for Windows, Linux, or Mac. The code is offered without warranty under the MIT License. Use it as you will personally or commercially, just give credit if you do.

- `ega2bmp.c` converts the given `.EGA` image into a Windows BMP format image
- `bmp2ega.c` converts the given Windows BMP format image into a `.EGA` image 

## The EGA File Format
The *EA-EGA* format is fairly simple, it comprises of an untagged header containing the width and height of the image, followed by RLE encoded packed 4 bits per pixel image data. The image is stored in reverse scanline order (left to right, bottom to top)

```c
typedef struct {
    uint16_t width;  // stored as image width - 1
    uint16_t height; // stored as image height - 1
    uint8_t rle_data[]; // RLE compressed pixel stream
} ega_file_t;
```

The EGA file format uses a code-based RLE compression scheme. The most signifcant bit of the code indicates if the encoded length represents a run, or a uncompressed string. The RLE encoding does not span scanlines, so any length encoding will terminate at teh end of a scanline, and start fresh on the next scanline.

```
code = get_byte()
if(code & 0x80)
    length = (code AND 0x7F) + 3
    value = get_byte()
    while(length)
        write_byte(value)
        length = length - 1
else
    length = code + 1
    while(length)
        write_byte(get_byte())
        length = length - 1
```

Once the data is uncompressed the pixels can be unpacked if desired. Each byte holds two pixels, the leftmost pixel is stored in the most significant nibble of the byte. Finally the data is stored in reverse scanline order, so to display correctly the data needs to be re-ordered or presented to the display from bottom to top.

