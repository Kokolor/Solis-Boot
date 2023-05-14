#include <stdint.h>
#include <stddef.h>

typedef struct
{
    unsigned char magic[2];
    unsigned char mode;
    unsigned char charsize;
} PSF1_HEADER;

typedef struct
{
    PSF1_HEADER *psf1_Header;
    void *glyphBuffer;
} PSF1_FONT;

typedef struct
{
    void (*putRectangle)(int x, int y, int width, int height, uint32_t color);
    void (*putPixel)(int x, int y, uint32_t color);
} BOOTLOADER_INTERFACE;

void _start(BOOTLOADER_INTERFACE *interface, PSF1_FONT *newFont)
{
    
    while (1)
        ;
}
