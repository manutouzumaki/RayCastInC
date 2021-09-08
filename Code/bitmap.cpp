static void *
ReadEntireFile(char *FileName, SIZE_T *FileSizePtr)
{
    HANDLE FileHandle =  CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, 0,
                                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if(FileHandle == INVALID_HANDLE_VALUE)
    {
        OutputDebugString("ERROR::OPENING::FILE\n");
        return 0;
    }
    LARGE_INTEGER FileSize;
    GetFileSizeEx(FileHandle, &FileSize);
    void *FileBuffer = VirtualAlloc(0, FileSize.QuadPart, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    *FileSizePtr = (SIZE_T)FileSize.QuadPart;
    if(!ReadFile(FileHandle, FileBuffer, (DWORD)FileSize.QuadPart, 0, 0))
    {
        OutputDebugString("ERROR::READING::FILE\n");
        return 0;
    }
    return FileBuffer;
}

static bit_map
LoadBMP(char *FileName)
{
    bit_map Result = {};   
    void *BMPFile = ReadEntireFile(FileName, &Result.Size);
    if(!BMPFile)
    {
        OutputDebugString("ERROR::LOADING::BMP\n");
        return Result;
    }

    bit_map_header *BMPHeader = (bit_map_header *)BMPFile;

    Result.Width = BMPHeader->Width;
    Result.Height = BMPHeader->Height;
    Result.BitsPerPixel = BMPHeader->BitsPerPixel;

    if(Result.BitsPerPixel == 8)
    {
        Result.ColorPalette = (unsigned int *)((unsigned char *)BMPFile + sizeof(bit_map_header));
        int StopHere = 0;
    }
    Result.Pixels = (void *)((unsigned char *)BMPFile + BMPHeader->BitmapOffset);

    return Result;
}

static BYTE *
CompressSprite(bit_map *SourceBitmap, DWORD TransparentColor)
{
    BYTE *SourceBits = (BYTE *)SourceBitmap->Pixels + (SourceBitmap->Width*(SourceBitmap->Height - 1));
    int SourceWidthBytes = SourceBitmap->Width;

    void *OutputBuffer = VirtualAlloc(0, SourceBitmap->Width*SourceBitmap->Height*sizeof(DWORD), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    DWORD *OutputRecord = (DWORD *)OutputBuffer;
    BYTE *OutpuByte;

    int X, Y;

    for(Y = 0;
        Y < SourceBitmap->Height;
        ++Y)
    {
        int Width = SourceBitmap->Width;
        enum state { InSkipRun, InCopyRun } State;
        BYTE *SourceByte = SourceBits;

        DWORD *NewLineRecord = OutputRecord++;
        int LineLength = 4;
        int CurrentRunLength = 1;

        OutpuByte = (BYTE *)(OutputRecord + 1);

        if(SourceBitmap->ColorPalette[*SourceByte ] == TransparentColor)
        {
            // we re staring a skip run
            State = InSkipRun;
            LineLength += 4; 
        }
        else
        {
            // Source id Data
            // we re staring a copy run
            State = InCopyRun;
            *OutpuByte++ = *SourceByte;
            LineLength += 5;
        }

        SourceByte++;

        for(X = 1;
            X < Width;
            ++X)
        {
            if(SourceBitmap->ColorPalette[*SourceByte ] == TransparentColor) 
            {
                if(State == InSkipRun)      // still in skip run
                {
                    CurrentRunLength++;
                }
                else                        // changing to skip run
                {
                    // write out copy record
                    *OutputRecord = COPYRUN(CurrentRunLength);
                    OutputRecord = (DWORD *)OutpuByte;

                    CurrentRunLength = 1;
                    State = InSkipRun;
                    LineLength += 4;
                }
            }
            else    // source is Data
            {
                if(State == InCopyRun)  // Still in copy run
                {
                    CurrentRunLength++;
                    *OutpuByte++ = *SourceByte;
                    LineLength++; 
                }
                else                    // changing to copy run
                {
                    // write out skip record
                    *OutputRecord = SKIPRUN(CurrentRunLength);
                    OutputRecord++;
                    OutpuByte = (BYTE *)(OutputRecord + 1);

                    CurrentRunLength = 1;
                    State = InCopyRun;
                    *OutpuByte++ = *SourceByte;
                    LineLength += 5;
                }
            }
            SourceByte++;
        }

        // finish off current record
        if(State == InSkipRun)
        {
            *OutputRecord = SKIPRUN(CurrentRunLength);
            OutputRecord++;
        }
        else    // InCopyRun
        {
            *OutputRecord = COPYRUN(CurrentRunLength);
            OutputRecord = (DWORD *)OutpuByte;
        }

        *NewLineRecord = NEWLINE(LineLength);
        
        SourceBits -= SourceWidthBytes;
    }

    return (BYTE *)OutputBuffer;
}

static void 
Bits8TransparentBltRLE(void *DestBuffer, BITMAPINFOHEADER *DestHeader,
                       int XDest, int YDest, bit_map SourceBitmap)
{
    unsigned int *DestPtr = (unsigned int *)DestBuffer;
    unsigned char *SourcePtr = (unsigned char *)SourceBitmap.Pixels;

    int DestDeltaScan, DestWidthBytes, DestRealHeight;
    int XSource = 0, YSource = 0;
    int Width, Height;

    DestWidthBytes = DestHeader->biWidth; 
    // check if we have bottom-up or top-down BackBuffer setup
    if(DestHeader->biHeight < 0)
    {
        // Dest if Top-Down
        DestRealHeight = -DestHeader->biHeight; // get pisitive height
        DestDeltaScan = DestWidthBytes; // travel down dest
    }
    else
    {
        // Dest if Bottom-Up
        DestRealHeight = DestHeader->biHeight; 
        DestDeltaScan = -DestWidthBytes; // travel down dest
        // point to top scanline
        DestPtr += DestHeader->biWidth*DestHeader->biHeight - DestWidthBytes;
    }

    // Clip the source to dest
    Width = SourceBitmap.Width;
    Height = SourceBitmap.Height;
    if(XDest < 0)
    {
        // left clipped
        Width += XDest;
        XSource = -XDest;
        XDest = 0;
    }
    if((XDest + Width) > DestHeader->biWidth)
    {
        // right clipped
        Width = DestHeader->biWidth - XDest;
    }
    if(YDest < 0)
    {
        // top clipped
        Height += YDest;
        YSource = -YDest;
        YDest = 0;
    }
    if((YDest + Height) > DestRealHeight)
    {
        // bottom clipped
        Height = DestRealHeight - YDest;
    }
    
    // step to starting source pixel
    DestPtr += (YDest*DestDeltaScan)+XDest;

    // account for processed span in delta scans
    DestDeltaScan -= Width;

    if((Height > 0) && (Width > 0))
    {
        // we have something to BLT
        int X, Y;
        DWORD *CurrentSourceScan = (DWORD *)SourcePtr;

        // prestep to starting source Y
        for(Y = 0;
            Y < YSource;
            ++Y)
        {
            CurrentSourceScan = (DWORD *)((BYTE *)CurrentSourceScan + RUNLENGTH(*CurrentSourceScan));
        }

        for(Y = 0;
            Y < Height;
            ++Y)
        {
            DWORD *CurrentSourceRecord = CurrentSourceScan + 1;

            // prestep to starting source X
            X = 0;
            while(X < XSource)
            {
                X += RUNLENGTH(*CurrentSourceRecord);
                if(X > XSource)
                {
                    // we need to partially process the current record
                    int Overlap = X - XSource;
                    int ActiveOverlap = (Overlap > Width) ? Width : Overlap;

                    if(ISCOPYRUN(*CurrentSourceRecord))
                    {
                        // copy overlap pixel to destination
                        // get pointer to data
                        BYTE *CopyRun = (BYTE *)CurrentSourceRecord + 4;

                        // prestep to desired pixel
                        CopyRun += RUNLENGTH(*CurrentSourceRecord) - Overlap;
                        
                        //memcpy(DestPtr, CopyRun, ActiveOverlap);
                        for(int MemoryIndex = 0;
                            MemoryIndex < ActiveOverlap;
                            ++MemoryIndex)
                        {
                            DestPtr[MemoryIndex] = SourceBitmap.ColorPalette[CopyRun[ActiveOverlap]];
                        } 
                    }
                    // Skip to next pixel
                    DestPtr += ActiveOverlap;
                }

                // skip to next record
                if(ISCOPYRUN(*CurrentSourceRecord))
                {
                    // skip any data bytes
                    CurrentSourceRecord = (DWORD *)((BYTE *)CurrentSourceRecord + RUNLENGTH(*CurrentSourceRecord));
                }
                CurrentSourceRecord++;
            }

            X = X - XSource;

            while(X < Width)
            {
                int RunLength = RUNLENGTH(*CurrentSourceRecord);
                int RemainingWidth = Width - X;
                int ActivePixels = (RunLength > RemainingWidth) ? RemainingWidth : RunLength;

                if(ISCOPYRUN(*CurrentSourceRecord))
                {
                    // copy pixels to destination
                    //get pointer data
                    BYTE *CopyRun = (BYTE *)CurrentSourceRecord + 4;
                    //memcpy(DestPtr, CopyRun, ActivePixels);
                    for(int MemoryIndex = 0;
                        MemoryIndex < ActivePixels;
                        ++MemoryIndex)
                    {
                        DestPtr[MemoryIndex] = SourceBitmap.ColorPalette[CopyRun[MemoryIndex]];
                    } 

                }
                // skip to the next dest pixel
                DestPtr += ActivePixels;

                // skip to the next record
                
                if(ISCOPYRUN(*CurrentSourceRecord))
                {
                    // skip any data bytes
                    CurrentSourceRecord = (DWORD *)((BYTE *)CurrentSourceRecord + RunLength);
                }

                CurrentSourceRecord++;  // skip record itself

                X += RunLength;
            }
            DestPtr += DestDeltaScan;

            CurrentSourceScan = (DWORD *)((BYTE *)CurrentSourceScan + RUNLENGTH(*CurrentSourceScan));
        }
    }
}


static void 
Bits8TransparentBlt(void *DestBuffer, BITMAPINFOHEADER *DestHeader,
               int XDest, int YDest, bit_map SourceBitmap, unsigned int TransparentMask)
{
    unsigned int *DestPtr = (unsigned int *)DestBuffer;
    unsigned char *SourcePtr = (unsigned char *)SourceBitmap.Pixels;

    int DestDeltaScan, DestWidthBytes, DestRealHeight;
    int SourceDeltaScan, XSource = 0, YSource = 0;
    int Width, Height;

    DestWidthBytes = DestHeader->biWidth; 
    // check if we have bottom-up or top-down BackBuffer setup
    if(DestHeader->biHeight < 0)
    {
        // Dest if Top-Down
        DestRealHeight = -DestHeader->biHeight; // get pisitive height
        DestDeltaScan = DestWidthBytes; // travel down dest
    }
    else
    {
        // Dest if Bottom-Up
        DestRealHeight = DestHeader->biHeight; 
        DestDeltaScan = -DestWidthBytes; // travel down dest
        // point to top scanline
        DestPtr += DestHeader->biWidth*DestHeader->biHeight - DestWidthBytes;
    }

    // Clip the source to dest
    Width = SourceBitmap.Width;
    Height = SourceBitmap.Height;
    if(XDest < 0)
    {
        // left clipped
        Width += XDest;
        XSource = -XDest;
        XDest = 0;
    }
    if((XDest + Width) > DestHeader->biWidth)
    {
        // right clipped
        Width = DestHeader->biWidth - XDest;
    }
    if(YDest < 0)
    {
        // top clipped
        Height += YDest;
        YSource = -YDest;
        YDest = 0;
    }
    if((YDest + Height) > DestRealHeight)
    {
        // bottom clipped
        Height = DestRealHeight - YDest;
    }
    
    SourceDeltaScan = SourceBitmap.Width;

    // step to starting source pixel
    SourcePtr += (SourceBitmap.Width*(SourceBitmap.Height - 1)) + XSource - (YSource*SourceDeltaScan);
    DestPtr += (YDest*DestDeltaScan)+XDest;

    // account for processed span in delta scans
    SourceDeltaScan += Width;
    DestDeltaScan -= Width;

    if((Height > 0) && (Width > 0))
    {
        // we have something to BLT
        for(int Y = 0;
            Y < Height;
            ++Y)
        {
            for(int X = 0;
                X < Width;
                ++X)
            {
                unsigned int Bits32SourceValue = SourceBitmap.ColorPalette[*SourcePtr];
                if((Bits32SourceValue & TransparentMask) != 0)
                {
                    *DestPtr = Bits32SourceValue; // copy the pixel
                }
                DestPtr++;
                SourcePtr++;
            }
            DestPtr += DestDeltaScan;
            SourcePtr -= SourceDeltaScan;
        }
    }

}

static void 
Bits32TransparentBlt(void *DestBuffer, BITMAPINFOHEADER *DestHeader,
               int XDest, int YDest, bit_map SourceBitmap)
{
    unsigned int *DestPtr = (unsigned int *)DestBuffer;
    unsigned int *SourcePtr = (unsigned int *)SourceBitmap.Pixels;
    int DestDeltaScan, DestWidthBytes, DestRealHeight;
    int SourceDeltaScan, XSource = 0, YSource = 0;
    int Width, Height;

    DestWidthBytes = DestHeader->biWidth; 
    // check if we have bottom-up or top-down BackBuffer setup
    if(DestHeader->biHeight < 0)
    {
        // Dest if Top-Down
        DestRealHeight = -DestHeader->biHeight; // get pisitive height
        DestDeltaScan = DestWidthBytes; // travel down dest
    }
    else
    {
        // Dest if Bottom-Up
        DestRealHeight = DestHeader->biHeight; 
        DestDeltaScan = -DestWidthBytes; // travel down dest
        // point to top scanline
        DestPtr += DestHeader->biWidth*DestHeader->biHeight - DestWidthBytes;
    }

    // Clip the source to dest
    Width = SourceBitmap.Width;
    Height = SourceBitmap.Height;
    if(XDest < 0)
    {
        // left clipped
        Width += XDest;
        XSource = -XDest;
        XDest = 0;
    }
    if((XDest + Width) > DestHeader->biWidth)
    {
        // right clipped
        Width = DestHeader->biWidth - XDest;
    }
    if(YDest < 0)
    {
        // top clipped
        Height += YDest;
        YSource = -YDest;
        YDest = 0;
    }
    if((YDest + Height) > DestRealHeight)
    {
        // bottom clipped
        Height = DestRealHeight - YDest;
    }
    
    SourceDeltaScan = SourceBitmap.Width;

    // step to starting source pixel
    SourcePtr += (SourceBitmap.Width*(SourceBitmap.Height - 1)) + XSource - (YSource*SourceDeltaScan);
    DestPtr += (YDest*DestDeltaScan)+XDest;

    // account for processed span in delta scans
    SourceDeltaScan += Width;
    DestDeltaScan -= Width;

    if((Height > 0) && (Width > 0))
    {
        // we have something to BLT
        for(int Y = 0;
            Y < Height;
            ++Y)
        {
            for(int X = 0;
                X < Width;
                ++X)
            {
                *DestPtr = *SourcePtr; // copy the pixel
                DestPtr++;
                SourcePtr++;
            }
            DestPtr += DestDeltaScan;
            SourcePtr -= SourceDeltaScan;
        }
    }

}
