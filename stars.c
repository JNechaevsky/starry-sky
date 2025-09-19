// MIT License
// 
// Copyright (c) 2024-2025 Polina "Aura" N.
// Copyright (c) 2024-2025 Julia Nechaevskaya
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Starry sky screensaver. ✨
// Date of creation: 18.11.2024
//
// Compile under MSYS:
//    windres resource.rc -o resource.o
//    gcc stars.c resource.o -o stars.exe -lgdi32 -mwindows
//
// Compile under Visual Studio Build Tools:
//    rc resource.rc
//    cl stars.c resource.res user32.lib gdi32.lib shell32.lib /link /SUBSYSTEM:WINDOWS


#include <windows.h>
#include <shellapi.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


typedef struct
{
    int x, y;
    int brightness;        // Current brightness (0-255)
    int target_brightness; // Target brightness (always 0 for fading)
    COLORREF color;        // Star color
} star_t;

#define BETWEEN(l, u, x) (((x) < (l)) ? (l) : ((x) > (u)) ? (u) : (x))
#define MAXSTARS 500

// Parameters loaded from ini file
int FULLSCREEN = 0;        // Run in fullscreen mode
int NUM_STARS = 100;       // Number of stars
int DELAY = 100;           // Delay in ms
int BRIGHTNESS_STEP = 15;  // Brightness step
int COLORED_STARS = 1;     // Colored stars
int BIG_STARS = 1;         // Large stars

// Global variables for window size
int window_width = 800;
int window_height = 600;
star_t stars[MAXSTARS];
RECT saved_rect = {0};      // Saved window size


// -----------------------------------------------------------------------------
// M_CheckParm
//  [PN] Simple command-line checker for WinMain-based apps
//  Returns the 1-based index of the parameter if found, 0 otherwise.
//  Accepts UTF-8 `check` and compares case-insensitively against wide argv.
// -----------------------------------------------------------------------------

static int M_CheckParm(const char *check)
{
    int argc = 0;
    LPWSTR *argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argvw)
    {
        return 0;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, check, -1, NULL, 0);
    if (wlen <= 0)
    {
        LocalFree(argvw);
        return 0;
    }

    wchar_t *wcheck = (wchar_t *)malloc((size_t)wlen * sizeof(wchar_t));
    if (!wcheck)
    {
        LocalFree(argvw);
        return 0;
    }

    MultiByteToWideChar(CP_UTF8, 0, check, -1, wcheck, wlen);

    int found = 0; // 0 means not found
    for (int i = 1; i < argc; i++)
    {
        if (_wcsicmp(argvw[i], wcheck) == 0)
        {
            // Return 1-based index like the original function
            found = i;
            break;
        }
    }

    free(wcheck);
    LocalFree(argvw);
    return found;
}

// -----------------------------------------------------------------------------
// M_CreateConfig
// Function to create ini file with default settings.
// -----------------------------------------------------------------------------

void M_CreateConfig(const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file)
    {
        fprintf(file, "# Run in full screen mode. (1 = yes, 0 = no)\n");
        fprintf(file, "FULLSCREEN 0\n\n");
        fprintf(file, "# Number of stars displayed on the screen. (0...500)\n");
        fprintf(file, "NUM_STARS 100\n\n");
        fprintf(file, "# Delay between frames in milliseconds. Affects animation speed. (0...1000)\n");
        fprintf(file, "DELAY 100\n\n");
        fprintf(file, "# Step by which brightness decreases. Affects fading smoothness. (1...255)\n");
        fprintf(file, "BRIGHTNESS_STEP 15\n\n");
        fprintf(file, "# Use colored stars. (1 = yes, 0 = grayscale)\n");
        fprintf(file, "COLORED_STARS 1\n\n");
        fprintf(file, "# Define star size. (0 = 1x1, 1 = 2x2, 2 = 4x4, etc., up to 4)\n");
        fprintf(file, "BIG_STARS 1\n");
        fclose(file);
    }
}

// -----------------------------------------------------------------------------
// M_LoadConfig
//  Read settings from stars.ini file.
// -----------------------------------------------------------------------------

void M_LoadConfig(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        M_CreateConfig(filename);
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), file))
    {
        // Remove line break
        line[strcspn(line, "\n")] = 0;

        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\0')
            continue;

        if (sscanf(line, "FULLSCREEN %d", &FULLSCREEN)) continue;
        if (sscanf(line, "NUM_STARS %d", &NUM_STARS)) continue;
        if (sscanf(line, "DELAY %d", &DELAY)) continue;
        if (sscanf(line, "BRIGHTNESS_STEP %d", &BRIGHTNESS_STEP)) continue;
        if (sscanf(line, "COLORED_STARS %d", &COLORED_STARS)) continue;
        if (sscanf(line, "BIG_STARS %d", &BIG_STARS)) continue;
    }

    fclose(file);
}

// -----------------------------------------------------------------------------
// M_CheckConfig
//  Check for safe limits of config file variables.
// -----------------------------------------------------------------------------

void M_CheckConfig(void)
{
    FULLSCREEN      = BETWEEN(0, 1,        FULLSCREEN);
    NUM_STARS       = BETWEEN(0, MAXSTARS, NUM_STARS);
    DELAY           = BETWEEN(0, 1000,     DELAY);
    BRIGHTNESS_STEP = BETWEEN(1, 255,      BRIGHTNESS_STEP);
    COLORED_STARS   = BETWEEN(0, 1,        COLORED_STARS);
    BIG_STARS       = BETWEEN(0, 4,        BIG_STARS);
}

// -----------------------------------------------------------------------------
// I_ToggleFullscreen
//  Toggle fullscreen mode.
// -----------------------------------------------------------------------------

void I_ToggleFullscreen(HWND hwnd, BOOL startup)
{
    static RECT saved_rect = {0}; // Saved window size

    if (FULLSCREEN && !startup)
    {
        // Exit fullscreen mode
        SetThreadExecutionState(ES_CONTINUOUS); // Reset screen saver/power off prevention
        SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP, saved_rect.left, saved_rect.top,
                     saved_rect.right - saved_rect.left, saved_rect.bottom - saved_rect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_NORMAL);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        FULLSCREEN = FALSE;
    }
    else
    {
        // Enter fullscreen mode
        MONITORINFO mi = {sizeof(mi)};
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        GetWindowRect(hwnd, &saved_rect); // Save current window size
        SetWindowLong(hwnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_MAXIMIZE);
        SetCursor(NULL);
        SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
        FULLSCREEN = TRUE;
    }

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

// -----------------------------------------------------------------------------
// R_ClearScreen
//  Clear screen.
// -----------------------------------------------------------------------------

void R_ClearScreen(HDC hdc, int width, int height)
{
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0)); // Black background
    RECT rect = {0, 0, width, height};
    FillRect(hdc, &rect, blackBrush);
    DeleteObject(blackBrush);
}

// -----------------------------------------------------------------------------
// R_InitializeStars
//  Initialize stars.
// -----------------------------------------------------------------------------

void R_InitializeStars(star_t stars[], int count, int max_x, int max_y)
{
    if (max_x <= 0 || max_y <= 0)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        stars[i].x = rand() % max_x;
        stars[i].y = rand() % max_y;
        stars[i].brightness = rand() % 256;
        stars[i].target_brightness = 0;
        stars[i].color = RGB(rand() % 256, rand() % 256, rand() % 256);
    }
}

// -----------------------------------------------------------------------------
// R_UpdateBrightness
//  Update star brightness for smooth fading.
// -----------------------------------------------------------------------------

void R_UpdateBrightness(star_t *star)
{
    if (star->brightness > star->target_brightness)
    {
        star->brightness -= BRIGHTNESS_STEP;
        if (star->brightness < star->target_brightness)
        {
            star->brightness = star->target_brightness;
        }
    }
}

// -----------------------------------------------------------------------------
// R_UpdateStars
//  Update stars.
// -----------------------------------------------------------------------------

void R_UpdateStars(star_t stars[], int count, int max_x, int max_y)
{
    if (max_x <= 0 || max_y <= 0)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        R_UpdateBrightness(&stars[i]);

        if (stars[i].brightness == 0)
        {
            stars[i].x = rand() % max_x;
            stars[i].y = rand() % max_y;
            stars[i].brightness = 255;
            stars[i].target_brightness = 0;
            stars[i].color = RGB(rand() % 256, rand() % 256, rand() % 256);
        }
    }
}

// -----------------------------------------------------------------------------
// R_DrawStars
//  Draw stars.
//-----------------------------------------------------------------------------

void R_DrawStars(HDC hdc, star_t stars[], int count)
{
    for (int i = 0; i < count; i++)
    {
        // If we are about to extinguish, paint black to erase the last faint pixel
        if (stars[i].brightness <= BRIGHTNESS_STEP)
        {
            // If you use BIG_STARS as a size (0=1x1, 1=2x2, 2=4x4...), compute size:
            const int size = (BIG_STARS > 0) ? (1 << BIG_STARS) : 1;

            if (size > 1)
            {
                RECT star_rect = { stars[i].x, stars[i].y, stars[i].x + size, stars[i].y + size };
                HBRUSH black = (HBRUSH)GetStockObject(BLACK_BRUSH);
                FillRect(hdc, &star_rect, black);
            }
            else
            {
                SetPixel(hdc, stars[i].x, stars[i].y, RGB(0, 0, 0));
            }
            continue; // skip normal drawing for this star on this frame
        }

        COLORREF star_color;
        if (COLORED_STARS)
        {
            const int r = GetRValue(stars[i].color) * stars[i].brightness / 255;
            const int g = GetGValue(stars[i].color) * stars[i].brightness / 255;
            const int b = GetBValue(stars[i].color) * stars[i].brightness / 255;
            star_color = RGB(r, g, b);
        }
        else
        {
            const int gray = stars[i].brightness;
            star_color = RGB(gray, gray, gray);
        }

        // If BIG_STARS encodes size (0=1x1, 1=2x2, 2=4x4, etc.)
        const int size = (BIG_STARS > 0) ? (1 << BIG_STARS) : 1;
        if (size > 1)
        {
            RECT star_rect = { stars[i].x, stars[i].y, stars[i].x + size, stars[i].y + size };
            HBRUSH brush = CreateSolidBrush(star_color);
            FillRect(hdc, &star_rect, brush);
            DeleteObject(brush);
        }
        else
        {
            SetPixel(hdc, stars[i].x, stars[i].y, star_color);
        }
    }
}

// -----------------------------------------------------------------------------
// WindowProc
//  Window procedure.
// -----------------------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HDC hdc;

    switch (uMsg)
    {
        case WM_CREATE:
            hdc = GetDC(hwnd);
            srand(time(NULL));
            R_InitializeStars(stars, NUM_STARS, window_width, window_height);
            break;

        case WM_LBUTTONDBLCLK:
            I_ToggleFullscreen(hwnd, false);
            break;

        case WM_SYSKEYDOWN: // Alt+Enter: toggle fullscreen (classic game shortcut)
        {
            // wParam == VK_RETURN and bit 29 of lParam set => ALT is held
            if (wParam == VK_RETURN && (lParam & (1 << 29)))
            {
                I_ToggleFullscreen(hwnd, FALSE);
                return 0; // handled (do not pass further)
            }
            // Not our combo — let DefWindowProc handle it (e.g. Alt+F4)
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_SYSCHAR: // Suppress system beep only for Alt+Enter
        {
            if (wParam == VK_RETURN)
                return 0; // handled (suppress beep)
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        case WM_SIZE:
            window_width = LOWORD(lParam);
            window_height = HIWORD(lParam);
            R_ClearScreen(hdc, window_width, window_height);

            if (window_width > 0 && window_height > 0)
            {
                R_InitializeStars(stars, NUM_STARS, window_width, window_height);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SETCURSOR:
            // In the client area, set the arrow cursor ourselves (if not fullscreen)
            if (!FULLSCREEN && LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
                return TRUE; // handled
            }
            // Fall through to default for non-client areas
            // break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

// -----------------------------------------------------------------------------
// WinMain
// -----------------------------------------------------------------------------

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Create console output window if "-console" parameter is present.
    if (M_CheckParm ("-console"))
    {
        // Allocate console.
        AllocConsole();
        SetConsoleTitle("Console");

        // Head text outputs.
        if (!freopen("CONIN$", "r", stdin))
            fprintf(stderr, "Failed to redirect stdin\n");
        if (!freopen("CONOUT$", "w", stdout))
            fprintf(stderr, "Failed to redirect stdout\n");
        if (!freopen("CONOUT$", "w", stderr))
            fprintf(stderr, "Failed to redirect stderr\n");

        // Set a proper codepage.
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    }

    const char CLASS_NAME[] = "DynamicStarrySky";

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(101));
    wc.hIconSm = wc.hIcon;

    wc.hbrBackground = CreateSolidBrush(0x00000000);
    wc.style = CS_DBLCLKS;

    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, "RegisterClassEx failed!", "Error", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        "Starry Sky",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, window_width, window_height,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    HDC hdc = GetDC(hwnd);

    MSG msg = {0};

    SetCursor(LoadCursor(NULL, IDC_ARROW));

    // Load settings
    M_LoadConfig("stars.ini");
    // Make sure that setting are valid
    M_CheckConfig();
    // Initialize star positions using the (possibly updated) configuration
    R_InitializeStars(stars, NUM_STARS, window_width, window_height);

    // Optionally start in full screen mode
    if (FULLSCREEN)
    {
        I_ToggleFullscreen(hwnd, true);
    }

    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                ReleaseDC(hwnd, hdc);
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        RECT rect;
        GetClientRect(hwnd, &rect);
        window_width = rect.right;
        window_height = rect.bottom;

        R_DrawStars(hdc, stars, NUM_STARS);
        R_UpdateStars(stars, NUM_STARS, window_width, window_height);

        Sleep(DELAY);
    }

    return 0;
}
