#include <efi.h>
#include <efilib.h>
#include <elf.h>

#define MAX_ENTRIES 10

typedef struct
{
    CHAR16 Name[128];
    CHAR16 Path[128];
} BOOT_ENTRY;

typedef struct
{
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} PSF1_HEADER;

typedef struct
{
    PSF1_HEADER *header;
    void *glyph_buffer;
} PSF1_FONT;

typedef struct
{
    void (*putRectangle)(int x, int y, int width, int height, uint32_t color);
    void (*putPixel)(int x, int y, uint32_t color);
} BOOTLOADER_INTERFACE;

BOOT_ENTRY Entries[MAX_ENTRIES];
UINTN EntryCount = 0;
EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;

CHAR16 *StrStr(CHAR16 *String, CHAR16 *SearchString)
{
    CHAR16 *FirstMatch;
    CHAR16 *SearchStringTmp;

    while (*String != L'\0')
    {
        SearchStringTmp = SearchString;
        FirstMatch = String;

        while ((*String == *SearchStringTmp) && (*String != L'\0'))
        {
            String++;
            SearchStringTmp++;
        }

        if (*SearchStringTmp == L'\0')
        {
            return FirstMatch;
        }

        if (*String == L'\0')
        {
            return NULL;
        }

        String = FirstMatch + 1;
    }

    return NULL;
}

size_t strlen(const char *str)
{
    const char *s;

    for (s = str; *s; ++s)
    {
    }

    return s - str;
}

void putPixel(int x, int y, uint32_t color)
{
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL pixel;

    pixel.Red = (UINT8)((color >> 16) & 0xFF);
    pixel.Green = (UINT8)((color >> 8) & 0xFF);
    pixel.Blue = (UINT8)(color & 0xFF);
    pixel.Reserved = 0;

    EFI_STATUS status = uefi_call_wrapper(
        gop->Blt,
        10,
        gop,
        &pixel,
        EfiBltBufferToVideo,
        0,
        0,
        x,
        y,
        1,
        1,
        0);

    if (EFI_ERROR(status))
    {
        Print(L"Failed to color the pixel\n");
    }
}

void putRectangle(int x, int y, int width, int height, uint32_t color)
{
    for (int i = y; i < y + height; i++)
    {
        for (int _i = x; _i < x + width; _i++)
        {
            putPixel(_i, i, color);
        }
    }
}

EFI_STATUS LoadPSF1Font(EFI_FILE_PROTOCOL *Root, CHAR16 *Path, PSF1_FONT *font)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *File;
    UINTN FileInfoSize = sizeof(EFI_FILE_INFO) + 1024;
    EFI_FILE_INFO *FileInfo = AllocatePool(FileInfoSize);
    UINT8 PSF1Magic[] = {0x36, 0x04};

    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        Print(L"Could not open the font file: %s\n", Path);
        return Status;
    }

    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status))
    {
        Print(L"Could not read the font file info: %s\n", Path);
        return Status;
    }

    UINTN size = sizeof(PSF1_HEADER);
    PSF1_HEADER *header = AllocateZeroPool(size);
    Status = uefi_call_wrapper(File->Read, 3, File, &size, header);
    if (EFI_ERROR(Status))
    {
        Print(L"Could not read the font file header: %s\n", Path);
        return Status;
    }

    if (header->magic[0] != PSF1Magic[0] || header->magic[1] != PSF1Magic[1])
    {
        Print(L"The file is not a valid .PSF font file.\n");
        return EFI_UNSUPPORTED;
    }

    UINTN glyphBufferSize = FileInfo->FileSize - sizeof(PSF1_HEADER);
    UINT8 *glyphBuffer = AllocateZeroPool(glyphBufferSize);
    Status = uefi_call_wrapper(File->Read, 3, File, &glyphBufferSize, glyphBuffer);
    if (EFI_ERROR(Status))
    {
        Print(L"Could not read the font glyph buffer: %s\n", Path);
        return Status;
    }

    font->header = header;
    font->glyph_buffer = glyphBuffer;

    uefi_call_wrapper(File->Close, 1, File);
    return EFI_SUCCESS;
}

void listRoot(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root;
    EFI_FILE_PROTOCOL *File;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_GUID SimpleFileSystemProtocol = {0x964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x0, 0xa0, 0xc9, 0x69, 0x72, 0x3b}};

    InitializeLib(ImageHandle, SystemTable);

    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &SimpleFileSystemProtocol, NULL, (void **)&SimpleFileSystem);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to locate simple file system protocol.\n");
        return;
    }

    Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 1, SimpleFileSystem, &Root);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to open root volume.\n");
        return;
    }

    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 1024;
    EFI_FILE_INFO *Buffer = AllocatePool(BufferSize);
    CHAR16 *Padding = AllocatePool(80 * sizeof(CHAR16));

    while (TRUE)
    {
        BufferSize = sizeof(EFI_FILE_INFO) + 1024;

        Status = uefi_call_wrapper(Root->Read, 3, Root, &BufferSize, Buffer);

        if (EFI_ERROR(Status) || BufferSize == 0)
            break;

        UINTN NameLength = StrLen(Buffer->FileName);
        for (UINTN i = 0; i < 80 - NameLength; ++i)
        {
            Padding[i] = L' ';
        }
        Padding[80 - NameLength] = L'\0';

        if (Buffer->Attribute & EFI_FILE_DIRECTORY)
            Print(L"%s%sDIR\n", Buffer->FileName, Padding);
        else
            Print(L"%s%sFILE\n", Buffer->FileName, Padding);
    }

    FreePool(Buffer);
    FreePool(Padding);

    uefi_call_wrapper(Root->Close, 1, Root);
}

void readBootConfig(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root, *File, *PathFile;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_GUID SimpleFileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
    CHAR16 *BootConfigFile = L"bootconfig.sboot";
    UINTN BufferSize = 1024;
    CHAR16 *Buffer = AllocatePool(BufferSize);

    InitializeLib(ImageHandle, SystemTable);

    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &SimpleFileSystemProtocol, NULL, (void **)&SimpleFileSystem);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to locate simple file system protocol.\n");
        return;
    }

    Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 1, SimpleFileSystem, &Root);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to open root volume.\n");
        return;
    }

    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, BootConfigFile, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to open bootconfig.sboot.\n");
        return;
    }

    Status = uefi_call_wrapper(File->Read, 3, File, &BufferSize, Buffer);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to read bootconfig.sboot.\n");
        return;
    }

    Buffer[BufferSize / sizeof(CHAR16)] = L'\0';

    if (StrStr(Buffer, L"listRoot") != NULL)
    {
        listRoot(ImageHandle, SystemTable);
    }

    CHAR16 *entryStart = StrStr(Buffer, L"entry/");
    while (entryStart != NULL && EntryCount < MAX_ENTRIES)
    {
        CHAR16 *pathStart = StrStr(entryStart, L" path/");
        if (pathStart != NULL)
        {
            *pathStart = L'\0';
            StrCpy(Entries[EntryCount].Name, entryStart + 6);
            *pathStart = L' ';

            CHAR16 *pathEnd = StrStr(pathStart, L"\n");
            if (pathEnd != NULL)
            {
                *pathEnd = L'\0';
                StrCpy(Entries[EntryCount].Path, pathStart + 6);
                *pathEnd = L'\n';
            }

            ++EntryCount;
        }

        entryStart = StrStr(entryStart + 6, L"entry/");
    }

    uefi_call_wrapper(File->Close, 1, File);
    FreePool(Buffer);
}

EFI_STATUS
LoadElf64(EFI_FILE_PROTOCOL *Root, CHAR16 *Path, Elf64_Ehdr **EntryPoint)
{
    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *File;
    UINTN FileSize = 0;
    UINT8 *FileData = NULL;

    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
        Print(L"Error opening kernel file: %r\n", Status);
        return Status;
    }

    EFI_FILE_INFO *FileInfo = LibFileInfo(File);
    if (FileInfo)
    {
        FileSize = FileInfo->FileSize;
        FreePool(FileInfo);
    }

    FileData = AllocatePool(FileSize);
    if (!FileData)
    {
        Print(L"Error allocating memory for kernel file.\n");
        return EFI_OUT_OF_RESOURCES;
    }

    Status = uefi_call_wrapper(File->Read, 3, File, &FileSize, FileData);
    if (EFI_ERROR(Status))
    {
        Print(L"Error reading kernel file: %r\n", Status);
        return Status;
    }

    Elf64_Ehdr *ElfHeader = (Elf64_Ehdr *)FileData;

    if (ElfHeader->e_ident[EI_MAG0] != ELFMAG0 || ElfHeader->e_ident[EI_MAG1] != ELFMAG1 ||
        ElfHeader->e_ident[EI_MAG2] != ELFMAG2 || ElfHeader->e_ident[EI_MAG3] != ELFMAG3)
    {
        Print(L"Invalid ELF magic\n");
        return EFI_UNSUPPORTED;
    }

    if (ElfHeader->e_ident[EI_CLASS] != ELFCLASS64)
    {
        Print(L"Unsupported ELF class\n");
        return EFI_UNSUPPORTED;
    }

    Elf64_Phdr *ProgramHeader = (Elf64_Phdr *)(FileData + ElfHeader->e_phoff);

    for (UINT16 i = 0; i < ElfHeader->e_phnum; i++)
    {
        if (ProgramHeader[i].p_type == PT_LOAD)
        {
            UINT64 SegmentSize = ProgramHeader[i].p_memsz;
            UINT64 *Src = (UINT64 *)(FileData + ProgramHeader[i].p_offset);
            UINT64 *Dst = (UINT64 *)ProgramHeader[i].p_vaddr;

            for (UINT64 j = 0; j < (SegmentSize / sizeof(UINT64)); j++)
            {
                Dst[j] = Src[j];
            }
        }
    }

    *EntryPoint = (Elf64_Ehdr *)ElfHeader->e_entry;

    uefi_call_wrapper(File->Close, 1, File);

    return EFI_SUCCESS;
}

void showMenu(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    UINTN Index;
    EFI_INPUT_KEY Key;
    UINTN CurrentSelection = 0;

    EFI_STATUS Status;
    EFI_FILE_PROTOCOL *Root;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
    EFI_GUID SimpleFileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

    Status = uefi_call_wrapper(BS->LocateProtocol, 3, &SimpleFileSystemProtocol, NULL, (void **)&SimpleFileSystem);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to locate simple file system protocol.\n");
        return;
    }

    Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 1, SimpleFileSystem, &Root);
    if (EFI_ERROR(Status))
    {
        Print(L"Unable to open root volume.\n");
        return;
    }

    while (1)
    {
        uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);

        for (UINTN i = 0; i < EntryCount; ++i)
        {
            Print(L"%s %s\n", (i == CurrentSelection) ? L">" : L" ", Entries[i].Name);
        }

        uefi_call_wrapper(BS->WaitForEvent, 3, 1, &ST->ConIn->WaitForKey, &Index);
        uefi_call_wrapper(ST->ConIn->ReadKeyStroke, 2, ST->ConIn, &Key);

        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN)
        {
            Print(L"Loading %s\n", Entries[CurrentSelection].Path);

            Elf64_Ehdr *EntryPoint = NULL;
            EFI_STATUS Status = LoadElf64(Root, Entries[CurrentSelection].Path, &EntryPoint);
            if (EFI_ERROR(Status))
            {
                Print(L"Failed to load %s\n", Entries[CurrentSelection].Name);
                continue;
            }
            BOOTLOADER_INTERFACE bootloaderInterface;
            bootloaderInterface.putRectangle = putRectangle;
            bootloaderInterface.putPixel = putPixel;
            PSF1_FONT *newFont;
            LoadPSF1Font(Root, L"zap-light16.psf", newFont);
            if (newFont == NULL)
            {
                Print(L"Font is not valid or is not found\n\r");
            }
            else
            {
                Print(L"Font found. char size = %d\n\r", newFont->header->charsize);
            }
            int (*KernelStart)(BOOTLOADER_INTERFACE *, PSF1_FONT *) = (int (*)())EntryPoint;

            KernelStart(&bootloaderInterface, newFont);
            break;
        }
        else if (Key.ScanCode == SCAN_DOWN)
        {
            // Down arrow key pressed
            CurrentSelection = (CurrentSelection + 1) % EntryCount;
        }
        else if (Key.ScanCode == SCAN_UP)
        {
            // Up arrow key pressed
            CurrentSelection = (CurrentSelection == 0) ? EntryCount - 1 : CurrentSelection - 1;
        }
    }
}

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    EFI_STATUS status = uefi_call_wrapper(BS->LocateProtocol, 3, &gEfiGraphicsOutputProtocolGuid, NULL, (void **)&gop);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to locate GOP\n");
        return status;
    }

    UINTN size_of_info = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

    UINTN size_of_info_key = 0;
    UINT32 key_pressed = 0;
    status = uefi_call_wrapper(gop->QueryMode, 4, gop, gop->Mode->Mode, &size_of_info, &info);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to query mode\n");
        return status;
    }

    status = uefi_call_wrapper(ST->ConIn->Reset, 2, ST->ConIn, FALSE);
    if (EFI_ERROR(status))
    {
        Print(L"Failed to reset console input\n");
        return status;
    }
    readBootConfig(ImageHandle, SystemTable);
    showMenu(ImageHandle, SystemTable);

    while (1)
        ;

    return EFI_SUCCESS;
}
