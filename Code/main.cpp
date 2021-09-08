#include <windows.h>
#include <stdio.h>
#include <math.h>
#include "bitmap.h"
#include "bitmap.cpp"

struct v2
{
    float X;
    float Y;
};

v2 operator+(v2& A, v2& B)
{
    v2 Result = {};
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    return Result;
}

v2 operator-(v2& A, v2& B)
{
    v2 Result = {};
    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    return Result;
}

v2 operator*(v2& A, float Value)
{
    v2 Result = {};
    Result.X = A.X * Value;
    Result.Y = A.Y * Value;
    return Result;
}

#define PI 3.14159265
#define TWO_PI 6.28318530

#define TILE_SIZE 64
#define MAP_NUM_ROWS 13
#define MAP_NUM_COLS 20

#define MINIMAP_SCALE_FACTOR 0.5

#define WND_WIDTH (MAP_NUM_COLS * TILE_SIZE)
#define WND_HEIGHT (MAP_NUM_ROWS * TILE_SIZE)

#define FOV_ANGLE (60 * (PI / 180))
#define WALL_STRIP_WIDTH 8.0
#define NUMBER_RAYS (WND_WIDTH/WALL_STRIP_WIDTH)

int Map[MAP_NUM_ROWS][MAP_NUM_COLS] = 
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

struct ray
{
    float Angle;
    float Distance;
    float XHit;
    float YHit;
    bool IsRayFacingUp;
    bool IsRayFacingDown;
    bool IsRayFacingLeft;
    bool IsRayFacingRight;
    bool ShouldRender;
};

struct player
{
    float X;
    float Y;
    int Radio;
    int TurnDirection; // -1 for left, +1 for right
    int WalkDirection; // -1 for back, +1 for front
    float RotationAngle;
    float WalkSpeed;
    float TurnSpeed;
    ray *Rays;
};

LRESULT CALLBACK WndProc(HWND   Window, UINT   Message, WPARAM WParam, LPARAM LParam);
void ProcesInputMessages(player *Player);

static bool GlobalRunning;

static void 
DrawRect(unsigned int *Buffer,
         int XMin, int YMin,
         int XMax, int YMax,
         unsigned int Color)
{
    if(XMin < 0)
    {
        XMin = 0;
    }
    if(YMin < 0)
    {
        YMin = 0;
    }
    if(XMax > WND_WIDTH)
    {
        XMax = WND_WIDTH;
    }
    if(YMax > WND_HEIGHT)
    {
        YMax = WND_HEIGHT;
    }

    unsigned int *Row = Buffer + (YMin*WND_WIDTH+XMin);
    for(int Y = YMin;
        Y < YMax;
        ++Y)
    {
        unsigned int *Pixels = Row;
        for(int X = XMin;
            X < XMax;
            ++X)
        {
            *Pixels++ = Color;
        }
        Row += WND_WIDTH;
    }
}

static void
DrawLine(unsigned int *Buffer,
         int AXPos, int AYPos,
         int BXPos, int BYPos,
         unsigned int Color)
{
    int XDelta = BXPos - AXPos;
    int YDelta = BYPos - AYPos;

    int SideLength = abs(XDelta) >= abs(YDelta) ? abs(XDelta) : abs(YDelta);

    float XInc = (float)XDelta / (float)SideLength;
    float YInc = (float)YDelta / (float)SideLength;

    float CurrentX = AXPos;
    float CurrentY = AYPos;

    for(int Index = 0;
        Index <= SideLength;
        ++Index)
    {
        if(CurrentX >= 0 && CurrentX < WND_WIDTH && CurrentY >= 0 && CurrentY < WND_HEIGHT)
        {
            unsigned int * Pixel = Buffer + ((int)CurrentY * WND_WIDTH) + (int)CurrentX;
            *Pixel = Color;
        }
        CurrentX += XInc; 
        CurrentY += YInc; 
    }
}

inline int
DistanceBetweenTwoPoints(int AXPos, int AYPos, int BXPos, int BYPos)
{
    int Result = 0;
    int XRel = BXPos - AXPos; 
    int YRel = BYPos - AYPos;
    Result = sqrt(XRel*XRel + YRel*YRel);
    return Result;
}

static void
DrawCircle(unsigned int *Buffer,
           int XPos, int YPos, int Radio,
           unsigned int Color)
{
    // first we create a quad 
    int XMin = XPos - Radio;
    int XMax = XPos + Radio + 1;
    int YMin = YPos - Radio;
    int YMax = YPos + Radio + 1;
    
    // clipping
    if(XMin < 0)
    {
        XMin = 0;
    }
    if(YMin < 0)
    {
        YMin = 0;
    }
    if(XMax > WND_WIDTH)
    {
        XMax = WND_WIDTH;
    }
    if(YMax > WND_HEIGHT)
    {
        YMax = WND_HEIGHT;
    }

    unsigned int *Row = Buffer + (YMin*WND_WIDTH+XMin);
    for(int Y = YMin;
        Y < YMax;
        ++Y)
    {
        unsigned int *Pixels = Row;
        for(int X = XMin;
            X < XMax;
            ++X)
        {
            if(DistanceBetweenTwoPoints(X, Y, XPos, YPos) <= Radio)
            { 
                *Pixels = Color;
            }
            ++Pixels;
        }
        Row += WND_WIDTH;
    }
}

static void
RenderMap(unsigned int *Buffer)
{
    for(int Y = 0;
        Y < MAP_NUM_ROWS;
        ++Y)
    {
        for(int X = 0;
            X < MAP_NUM_COLS;
            ++X)
        {
            int TileX = X * TILE_SIZE;
            int TileY = Y * TILE_SIZE;
            unsigned int Color = Map[Y][X] != 0 ? 0xFF222222 : 0xFFFFFFDD;
            DrawRect(Buffer,
                     TileX * MINIMAP_SCALE_FACTOR,
                     TileY * MINIMAP_SCALE_FACTOR,
                     TileX * MINIMAP_SCALE_FACTOR + TILE_SIZE * MINIMAP_SCALE_FACTOR,
                     TileY * MINIMAP_SCALE_FACTOR + TILE_SIZE * MINIMAP_SCALE_FACTOR,
                     Color);
        }
    }
}

static void
RenderPlayer(unsigned int *Buffer, player *Player)
{
    DrawLine(Buffer,
             MINIMAP_SCALE_FACTOR * Player->X,
             MINIMAP_SCALE_FACTOR * Player->Y,
             MINIMAP_SCALE_FACTOR * Player->X + cos(Player->RotationAngle) * (40 * MINIMAP_SCALE_FACTOR),
             MINIMAP_SCALE_FACTOR * Player->Y + sin(Player->RotationAngle) * (40 * MINIMAP_SCALE_FACTOR),
             0xFFFF00FF);

    DrawCircle(Buffer,
               Player->X * MINIMAP_SCALE_FACTOR,
               Player->Y * MINIMAP_SCALE_FACTOR,
               Player->Radio * MINIMAP_SCALE_FACTOR,
               0xFFFF0000);
}

// TODO: Finish this function...
static void
InitRays(player  *Player)
{
    Player->Rays = (ray *)VirtualAlloc(0, NUMBER_RAYS*sizeof(ray), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}



static float
NormalizeAngle(float Angle)
{
    Angle = fmodf(Angle, 2.0f*PI);
    if(Angle < 0)
    {
        Angle = (2 * PI) + Angle;
    }
    return Angle;
}

static bool
IsMapPositionEmpty(float X, float Y)
{
    if(X < 0 || X > WND_WIDTH || Y < 0 || Y > WND_HEIGHT)
    {
        return false;
    }
    bool Result = false;
    int TileX = floorf(X / TILE_SIZE);
    int TileY = floorf(Y / TILE_SIZE);
    Result = Map[TileY][TileX] != 0 ? false : true;
    return Result; 
}

inline float
Length(float Ax, float Ay)
{
    float Result = 0;
    Result = sqrt(Ax*Ax + Ay*Ay);
    return Result;    
}

static void
CastRay(ray *Ray, player *Player)
{
    // First we see where the ray is pointing to
    Ray->IsRayFacingUp    = (Ray->Angle > PI && Ray->Angle < 2*PI);
    Ray->IsRayFacingDown  = !Ray->IsRayFacingUp;
    Ray->IsRayFacingRight = (Ray->Angle < 0.5f*PI || Ray->Angle > 1.5f*PI);
    Ray->IsRayFacingLeft  = !Ray->IsRayFacingRight;

    ///////////////////////////////////////////
    // HORIZONTAL HIT.
    ///////////////////////////////////////////
    bool FoundHorizontalIntersection = false;
    float YHorizontalIntersection = floorf(Player->Y/TILE_SIZE) * TILE_SIZE;
    YHorizontalIntersection += Ray->IsRayFacingDown ? TILE_SIZE : 0;
    float XHorizontalIntersection = Player->X + ((YHorizontalIntersection - Player->Y) /  tanf(Ray->Angle));

    float YIncrement = TILE_SIZE;
    YIncrement *= Ray->IsRayFacingUp ? -1.0f : 1.0f;
    float XIncrement = TILE_SIZE / tanf(Ray->Angle); 
    XIncrement *= Ray->IsRayFacingLeft  && XIncrement > 0 ? -1.0f : 1.0f;
    XIncrement *= Ray->IsRayFacingRight && XIncrement < 0 ? -1.0f : 1.0f;

    float NextHorizontalTouchX = XHorizontalIntersection;
    float NextHorizontalTouchY = YHorizontalIntersection; 
    NextHorizontalTouchY -= Ray->IsRayFacingUp ? 1.0f : 0.0f;
    while(NextHorizontalTouchX >= 0 && NextHorizontalTouchX <= WND_WIDTH &&
          NextHorizontalTouchY >= 0 && NextHorizontalTouchY <= WND_HEIGHT)
    {
        if(!IsMapPositionEmpty((int)NextHorizontalTouchX, (int)NextHorizontalTouchY))
        {
            FoundHorizontalIntersection = true;
            break;
        }
        else
        {
            NextHorizontalTouchX += XIncrement;
            NextHorizontalTouchY += YIncrement;
        }
    }

    ///////////////////////////////////////////
    // VERTICAL HIT.
    ///////////////////////////////////////////
    bool FoundVerticalIntersection = false;
    float XVerticalIntersection = floorf(Player->X/TILE_SIZE) * TILE_SIZE;
    XVerticalIntersection += Ray->IsRayFacingRight ? TILE_SIZE : 0;
    float YVerticalIntersection = Player->Y + (tanf(Ray->Angle)*(XVerticalIntersection - Player->X));

    XIncrement = TILE_SIZE;
    XIncrement *= Ray->IsRayFacingLeft ? -1.0f : 1.0f;
    
    YIncrement = tanf(Ray->Angle) * TILE_SIZE; 
    YIncrement *= Ray->IsRayFacingUp  && YIncrement > 0 ? -1.0f : 1.0f;
    YIncrement *= Ray->IsRayFacingDown && YIncrement < 0 ? -1.0f : 1.0f;

    float NextVerticalTouchX = XVerticalIntersection;
    float NextVerticalTouchY = YVerticalIntersection; 
    NextVerticalTouchX -= Ray->IsRayFacingLeft ? 1.0f : 0.0f;
    while(NextVerticalTouchX >= 0 && NextVerticalTouchX <= WND_WIDTH &&
          NextVerticalTouchY >= 0 && NextVerticalTouchY <= WND_HEIGHT)
    {
        if(!IsMapPositionEmpty((int)NextVerticalTouchX, (int)NextVerticalTouchY))
        {
            FoundVerticalIntersection = true;
            break;
        }
        else
        {
            NextVerticalTouchX += XIncrement;
            NextVerticalTouchY += YIncrement;
        }
    }

    Ray->ShouldRender = FoundHorizontalIntersection || FoundVerticalIntersection;
    float HDistance = Length(fabs(Player->X - NextHorizontalTouchX), fabs(Player->Y - NextHorizontalTouchY));
    float VDistance = Length(fabs(Player->X - NextVerticalTouchX), fabs(Player->Y - NextVerticalTouchY));
    if(HDistance < VDistance)
    {
        Ray->Distance = HDistance;
        Ray->XHit = NextHorizontalTouchX;
        Ray->YHit = NextHorizontalTouchY;
    }
    else
    {
        Ray->Distance = VDistance;
        Ray->XHit = NextVerticalTouchX;
        Ray->YHit = NextVerticalTouchY;
    }

}

static void 
CastAllRays(player *Player)
{
    float Angle = Player->RotationAngle - (FOV_ANGLE/2);
    for(int Index = 0;
        Index < NUMBER_RAYS;
        ++Index)
    {
        ray *Ray = Player->Rays + Index;   
        Ray->Angle = NormalizeAngle(Angle);
        
        CastRay(Ray, Player);

        Angle += FOV_ANGLE/NUMBER_RAYS;
    }
}

static void 
RenderRays(unsigned int *Buffer, player *Player)
{
    float Angle = Player->RotationAngle - (FOV_ANGLE/2);
    for(int Index = 0;
        Index < NUMBER_RAYS;
        ++Index)
    { 
        ray *Ray = Player->Rays + Index;   
        if(Ray->ShouldRender)
        {
            DrawLine(Buffer, 
            MINIMAP_SCALE_FACTOR * Player->X,
            MINIMAP_SCALE_FACTOR * Player->Y,
            MINIMAP_SCALE_FACTOR * Ray->XHit,
            MINIMAP_SCALE_FACTOR * Ray->YHit,
            0xFF00FF00);
        }
        
    }
}

static void
MovePlayer(player *Player, float DeltaTime)
{
    Player->RotationAngle += Player->TurnDirection * Player->TurnSpeed * DeltaTime;
    float MoveStep = Player->WalkDirection * Player->WalkSpeed * DeltaTime;

    float NewPlayerX = Player->X + cosf(Player->RotationAngle) * MoveStep;
    float NewPlayerY = Player->Y + sinf(Player->RotationAngle) * MoveStep;

    if(IsMapPositionEmpty(NewPlayerX, NewPlayerY))
    {
        Player->X = NewPlayerX;
        Player->Y = NewPlayerY;
    }
}

int WINAPI WinMain(HINSTANCE Instance,
                   HINSTANCE PrevInstance,
                   LPSTR     lpCmdLine,
                   int       nShowCmd)
{
    WNDCLASSEX WindowClass = { 0 };
    WindowClass.cbSize = sizeof( WNDCLASSEX ) ;
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = WndProc;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
    WindowClass.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    WindowClass.lpszMenuName = NULL;
    WindowClass.lpszClassName = "Transparent BLTs";

    RegisterClassEx(&WindowClass);

    RECT Rect = { 0, 0, WND_WIDTH, WND_HEIGHT };
    AdjustWindowRect( &Rect, WS_OVERLAPPEDWINDOW, FALSE );

    HWND Window = CreateWindowA("Transparent BLTs",
                                "Transparent BLTs",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                Rect.right - Rect.left,
                                Rect.bottom - Rect.top,
                                NULL, NULL, Instance, NULL);
    if(Window)
    {  
        LARGE_INTEGER Frequency = {};
        QueryPerformanceFrequency(&Frequency);
        bool SleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);
        float FPS = 30.0f;
        float TARGET_SECONDS_FRAME = (1.0f / FPS);

        RECT ClientDimensions = {};
        GetClientRect(Window, &ClientDimensions);
        unsigned int Width  = ClientDimensions.right - ClientDimensions.left;
        unsigned int Height = ClientDimensions.bottom - ClientDimensions.top;
        int BackBufferSizeInBytes = Width*Height*sizeof(unsigned int);
        // Alloc memory for the back buffer.
        void *BackBuffer = VirtualAlloc(0, BackBufferSizeInBytes, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        
        BITMAPINFO BackBufferInfo = {};
        BackBufferInfo.bmiHeader.biSize = sizeof(BackBufferInfo.bmiHeader);
        BackBufferInfo.bmiHeader.biWidth = Width;
        BackBufferInfo.bmiHeader.biHeight = -Height;
        BackBufferInfo.bmiHeader.biPlanes = 1;
        BackBufferInfo.bmiHeader.biBitCount = 32;
        BackBufferInfo.bmiHeader.biCompression = BI_RGB;

        HDC DeviceContext = GetDC(Window);
        HDC BufferContext = CreateCompatibleDC(DeviceContext);
        HBITMAP BackBufferHandle = CreateDIBSection(BufferContext, &BackBufferInfo,
                                                    DIB_RGB_COLORS, &BackBuffer, NULL, NULL);    

        SelectObject(BufferContext, BackBufferHandle);

        bit_map Sabrina32BMP = LoadBMP("../Data/Sabrina.bmp");
        bit_map Sabrina8BMP = LoadBMP("../Data/Sabrina1.bmp");
        bit_map Camila32BMP = LoadBMP("../Data/Camila.bmp");
        bit_map Camila8BMP = LoadBMP("../Data/Camila1.bmp");
        bit_map Doggie8BMP = LoadBMP("../Data/DOGGIE4.BMP");
        Doggie8BMP.Pixels = (void *)CompressSprite(&Doggie8BMP, 0xFF000100);
        
        player Player = {};
        Player.X = WND_WIDTH / 2;
        Player.Y = WND_HEIGHT / 2;
        Player.Radio = 4;
        Player.TurnDirection = 0;
        Player.WalkDirection = 0;
        Player.RotationAngle = PI / 2;
        Player.WalkSpeed = 100;
        Player.TurnSpeed = 90 * (PI / 180);

        InitRays(&Player);

        GlobalRunning = true;
        ShowWindow(Window, nShowCmd);
        
        LARGE_INTEGER LastCount = {};
        QueryPerformanceCounter(&LastCount);
    
        while(GlobalRunning)
        { 
#if 1
            // NOT TESTED SLEEP AND FPS FIXING
            // --------------------------------------------------------
            LARGE_INTEGER WorkCount = {};
            QueryPerformanceCounter(&WorkCount);
            unsigned long long DeltaWorkCount = WorkCount.QuadPart - LastCount.QuadPart;            
            float SecondElapseForFrame = ((float)DeltaWorkCount / (float)Frequency.QuadPart);
            while(SecondElapseForFrame < TARGET_SECONDS_FRAME)
            {                
                if(SleepIsGranular)
                {
                    DWORD SleepMS = (DWORD)(1000.0f*(TARGET_SECONDS_FRAME-SecondElapseForFrame));
                    if(SleepMS > 0)
                    {
                        Sleep(SleepMS);
                    }
                    QueryPerformanceCounter(&WorkCount);
                    DeltaWorkCount = WorkCount.QuadPart - LastCount.QuadPart;            
                    SecondElapseForFrame = ((float)DeltaWorkCount / (float)Frequency.QuadPart);
                }
            }
            // --------------------------------------------------------
#endif
            
            LARGE_INTEGER ActualCount = {};
            QueryPerformanceCounter(&ActualCount);
            unsigned long long DeltaCount = ActualCount.QuadPart - LastCount.QuadPart;            
            float DeltaTime = ((float)DeltaCount / (float)Frequency.QuadPart);

            ProcesInputMessages(&Player);
            unsigned int *Buffer = (unsigned int *)BackBuffer;

            MovePlayer(&Player, DeltaTime);
            CastAllRays(&Player);

            for(int Y = 0;
                Y < Height;
                ++Y)
            {
                for(int X = 0;
                    X < Width;
                    ++X)
                {
                    Buffer[Y*Width+X] = 0xFF000000; 
                }
            }
#if 0
            // TEST CODE DELETE THIST LATER...
            // --------------------------------------------------------
            Bits32TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                            0, 0, Sabrina32BMP);
            Bits8TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                                256, 0, Sabrina8BMP, 0xFF000000);
            Bits32TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                                 0, 256, Camila32BMP);
            Bits8TransparentBlt(BackBuffer, &BackBufferInfo.bmiHeader,
                           128, 256, Camila8BMP, 0xFF000000);
 
            Bits8TransparentBltRLE(BackBuffer, &BackBufferInfo.bmiHeader,
                                   300, 200, Doggie8BMP);

            DrawRect(Buffer, 100, 350, 150, 450, 0xFFFF0000);
            DrawLine(Buffer, 0, 0, 100, 50, 0xFF00FF00);
            DrawLine(Buffer, 0, 50, 100, 0, 0xFFFF0000);
            DrawCircle(Buffer, 100, 100, 100, 0xFF0000FF);

            static float Time = 0.0f;
            float XClockPos = (cosf(Time) * 100) + 100;
            float YClockPos = (sinf(Time) * 100) + 100;
            DrawLine(Buffer, 100, 100, XClockPos, YClockPos, 0xFFFF00FF);
            DrawCircle(Buffer, XClockPos, YClockPos, 5, 0xFFFFFF00);
            Time += 0.01f;
            // --------------------------------------------------------
#endif     

            RenderMap(Buffer);
            RenderRays(Buffer, &Player);
            RenderPlayer(Buffer, &Player);


            BitBlt(DeviceContext, NULL, NULL, Width, Height, BufferContext, NULL, NULL, SRCCOPY);

            LastCount = ActualCount;

        }   
    }
    return 0;
}

LRESULT CALLBACK WndProc(HWND   Window,
                         UINT   Message,
                         WPARAM WParam,
                         LPARAM LParam)
{
    LRESULT Result = {};
    switch(Message)
    {
        case WM_CLOSE:
        {
            GlobalRunning = false;
        }break;
        case WM_DESTROY:
        {
            GlobalRunning = false;
        }break;
        default:
        {
           Result = DefWindowProcA(Window, Message, WParam, LParam);
        }break; 
    }
    return Result;
}

void 
ProcesInputMessages(player *Player)
{
    MSG Message = {};
    while(PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch(Message.message)
        {
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            {
                DWORD VKCode = (DWORD)Message.wParam;
                if(VKCode == 'W')
                {
                    Player->WalkDirection = +1;
                }
                if(VKCode == 'S')
                {
                    Player->WalkDirection = -1;
                }
                if(VKCode == 'D')
                {
                    Player->TurnDirection = +1;
                }
                if(VKCode == 'A')
                {
                    Player->TurnDirection = -1;
                }
            }break;
            case WM_KEYUP:
            case WM_SYSKEYUP:
            { 
                DWORD VKCode = (DWORD)Message.wParam; 
                if(VKCode == 'W')
                {
                    Player->WalkDirection = 0;
                }
                if(VKCode == 'S')
                {
                    Player->WalkDirection = 0;
                }
                if(VKCode == 'D')
                {
                    Player->TurnDirection = 0;
                }
                if(VKCode == 'A')
                {
                    Player->TurnDirection = 0;
                }
            }break;
            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);   
            }break;
        }
    }
}
