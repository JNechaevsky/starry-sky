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


#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


typedef struct
{
    int x, y;
    int brightness;        // Current brightness (0-255)
    int target_brightness; // Target brightness (always 0 for fading)
    COLORREF color;        // Star color
} Star;

// Parameters loaded from ini file
int NUM_STARS = 100;       // Number of stars
int DELAY = 100;           // Delay in ms
int BRIGHTNESS_STEP = 15;  // Brightness step
int COLORED_STARS = 1;     // Colored stars
int BIG_STARS = 1;         // Large stars

// Global variables for window size
int window_width = 800;
int window_height = 600;
Star stars[300];
RECT saved_rect = {0};      // Saved window size
BOOL is_fullscreen = FALSE; // Fullscreen mode indicator

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

// Function to create ini file with default settings
void create_default_settings_file(const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (file)
    {
        fprintf(file, "# Number of stars displayed on the screen\n");
        fprintf(file, "NUM_STARS 300\n\n");
        fprintf(file, "# Delay between frames in milliseconds (affects animation speed)\n");
        fprintf(file, "DELAY 50\n\n");
        fprintf(file, "# Step by which brightness decreases (affects fading smoothness)\n");
        fprintf(file, "BRIGHTNESS_STEP 15\n\n");
        fprintf(file, "# Use colored stars (1 = yes, 0 = grayscale)\n");
        fprintf(file, "COLORED_STARS 1\n\n");
        fprintf(file, "# Draw large stars as small squares (1 = yes, 0 = single pixel)\n");
        fprintf(file, "BIG_STARS 1\n");
        fclose(file);
    }
}

// Reading settings from stars.ini file
void load_settings_from_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        create_default_settings_file(filename);
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

        if (sscanf(line, "NUM_STARS %d", &NUM_STARS)) continue;
        if (sscanf(line, "DELAY %d", &DELAY)) continue;
        if (sscanf(line, "BRIGHTNESS_STEP %d", &BRIGHTNESS_STEP)) continue;
        if (sscanf(line, "COLORED_STARS %d", &COLORED_STARS)) continue;
        if (sscanf(line, "BIG_STARS %d", &BIG_STARS)) continue;
    }

    fclose(file);
}

// Clear screen
void clear_screen(HDC hdc, int width, int height)
{
    HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0)); // Black background
    RECT rect = {0, 0, width, height};
    FillRect(hdc, &rect, blackBrush);
    DeleteObject(blackBrush);
}

// Draw stars
void draw_stars(HDC hdc, Star stars[], int count)
{
    for (int i = 0; i < count; i++)
    {
        COLORREF star_color;

        if (COLORED_STARS)
        {
            int r = GetRValue(stars[i].color) * stars[i].brightness / 255;
            int g = GetGValue(stars[i].color) * stars[i].brightness / 255;
            int b = GetBValue(stars[i].color) * stars[i].brightness / 255;
            star_color = RGB(r, g, b);
        }
        else
        {
            int gray = stars[i].brightness;
            star_color = RGB(gray, gray, gray);
        }

        if (BIG_STARS)
        {
            RECT star_rect = {stars[i].x, stars[i].y, stars[i].x + 2, stars[i].y + 2};
            HBRUSH star_brush = CreateSolidBrush(star_color);
            FillRect(hdc, &star_rect, star_brush);
            DeleteObject(star_brush);
        }
        else
        {
            SetPixel(hdc, stars[i].x, stars[i].y, star_color);
        }
    }
}

// Toggle fullscreen mode
void toggle_fullscreen(HWND hwnd)
{
    static RECT saved_rect = {0}; // Saved window size

    if (is_fullscreen)
    {
        // Exit fullscreen mode
        SetThreadExecutionState(ES_CONTINUOUS); // Reset screen saver/power off prevention
        SetWindowLong(hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP, saved_rect.left, saved_rect.top,
                     saved_rect.right - saved_rect.left, saved_rect.bottom - saved_rect.top,
                     SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        ShowWindow(hwnd, SW_NORMAL);
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        is_fullscreen = FALSE;
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
        is_fullscreen = TRUE;
    }

    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
}

// Update star brightness for smooth fading
void update_brightness(Star *star)
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

// Update stars
void update_stars(Star stars[], int count, int max_x, int max_y)
{
    if (max_x <= 0 || max_y <= 0)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        update_brightness(&stars[i]);

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

// Initialize stars
void initialize_stars(Star stars[], int count, int max_x, int max_y)
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

// Window procedure
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HDC hdc;

    switch (uMsg)
    {
        case WM_CREATE:
            hdc = GetDC(hwnd);
            srand(time(NULL));
            initialize_stars(stars, NUM_STARS, window_width, window_height);
            break;

        case WM_LBUTTONDBLCLK:
            toggle_fullscreen(hwnd);
            break;

        case WM_SIZE:
            window_width = LOWORD(lParam);
            window_height = HIWORD(lParam);
            clear_screen(hdc, window_width, window_height);

            if (window_width > 0 && window_height > 0)
            {
                initialize_stars(stars, NUM_STARS, window_width, window_height);
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        case WM_SETCURSOR:
            // In the client area, set the arrow cursor ourselves (if not fullscreen)
            if (!is_fullscreen && LOWORD(lParam) == HTCLIENT)
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

// WinMain
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const char CLASS_NAME[] = "DynamicStarrySky";

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    wc.hbrBackground = CreateSolidBrush(0x00000000);
    wc.style = CS_DBLCLKS;
    wc.hIconSm = NULL;

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

    load_settings_from_file("stars.ini"); // Load settings

    // [JN] Create console output window if "-console" parameter is present.
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

        draw_stars(hdc, stars, NUM_STARS);
        update_stars(stars, NUM_STARS, window_width, window_height);

        Sleep(DELAY);
    }

    return 0;
}