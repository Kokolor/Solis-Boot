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
    void *baseAddress;
    size_t bufferSize;
    unsigned int width;
    unsigned int height;
    unsigned int pixelsPerScanLine;
} FRAMEBUFFER;

typedef struct
{
    unsigned int cursorX;
    unsigned int cursorY;
    unsigned int colour;
    PSF1_FONT *font;
    FRAMEBUFFER *framebuffer;
} TERMINAL_STATE;

size_t strlen(const char *str)
{
    const char *s;
    for (s = str; *s; ++s)
        ;
    return s - str;
}

void initTerminal(TERMINAL_STATE *terminal, FRAMEBUFFER *framebuffer, PSF1_FONT *psfFont)
{
    terminal->cursorX = 0;
    terminal->cursorY = 0;
    terminal->colour = 0x00009f3b;
    terminal->font = psfFont;
    terminal->framebuffer = framebuffer;
}

void putChar(TERMINAL_STATE *terminal, char character)
{
    if (character == '\n')
    {
        terminal->cursorX = 0;
        terminal->cursorY += 16;
        return;
    }

    unsigned int *pixelPtr = (unsigned int *)terminal->framebuffer->baseAddress;
    char *fontPtr = terminal->font->glyphBuffer + (character * terminal->font->psf1_Header->charsize);
    for (unsigned long _y = terminal->cursorY; _y < terminal->cursorY + 16; _y++)
    {
        for (unsigned long _x = terminal->cursorX; _x < terminal->cursorX + 8; _x++)
        {
            if ((*fontPtr & (0b10000000 >> (_x - terminal->cursorX))) > 0)
            {
                *(unsigned int *)(pixelPtr + _x + (_y * terminal->framebuffer->pixelsPerScanLine)) = terminal->colour;
            }
        }
        fontPtr++;
    }
    terminal->cursorX += 8;

    if (terminal->cursorX > terminal->framebuffer->width)
    {
        terminal->cursorX = 0;
        terminal->cursorY += 16;
    }
}

void putString(TERMINAL_STATE *terminal, char *str)
{
    for (size_t i = 0; i < strlen(str); i++)
        putChar(terminal, str[i]);
}

void _start(FRAMEBUFFER *framebuffer, PSF1_FONT *psfFont)
{
    TERMINAL_STATE *terminal;
    initTerminal(terminal, framebuffer, psfFont);
    putString(terminal, "Solis\n");

    while (1)
        ;
}
