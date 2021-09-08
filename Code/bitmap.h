#ifndef BITMAP_H
#define BITMAP_H

#pragma pack(push, 1)
struct bit_map_header
{
	WORD   FileType;        /* File type, always 4D42h ("BM") */
	DWORD  FileSize;        /* Size of the file in bytes */
	WORD   Reserved1;       /* Always 0 */
	WORD   Reserved2;       /* Always 0 */
	DWORD  BitmapOffset;    /* Starting position of image data in bytes */

	DWORD  Size;            /* Size of this header in bytes */
	LONG   Width;           /* Image width in pixels */
	LONG   Height;          /* Image height in pixels */
	WORD   Planes;          /* Number of color planes */
	WORD   BitsPerPixel;    /* Number of bits per pixel */
    DWORD Compression;     /* Compression methods used */
	DWORD SizeOfBitmap;    /* Size of bitmap in bytes */
	LONG  HorzResolution;  /* Horizontal resolution in pixels per meter */
	LONG  VertResolution;  /* Vertical resolution in pixels per meter */
	DWORD ColorsUsed;      /* Number of colors in the image */
	DWORD ColorsImportant; /* Minimum number of important colors */
	
};
#pragma pack(pop)

struct bit_map
{
    void *Pixels;
    SIZE_T Size;
    int Width;
    int Height;
    int BitsPerPixel;
    unsigned int *ColorPalette;
};

// RLE Compressor
#define NEWLINE( Length ) ((DWORD)(0x00000000 | (short unsigned)(Length)))
#define SKIPRUN( Length ) ((DWORD)(0x00010000 | (short unsigned)(Length)))
#define COPYRUN( Length ) ((DWORD)(0x00020000 | (short unsigned)(Length)))

#define ISSKIPRUN( Record ) (int)((((DWORD)(Record)) & 0xFFFF0000) == 0x00010000)
#define ISCOPYRUN( Record ) (int)((((DWORD)(Record)) & 0xFFFF0000) == 0x00020000)
#define RUNLENGTH( Record ) (int)(((DWORD)(Record))  & 0x0000FFFF)

#endif
