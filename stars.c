// MIT License
// 
// Copyright (c) 2024-2026 Polina "Aura" N.
// Copyright (c) 2024-2026 Julia Nechaevskaya
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
//    windres resource.rc -O coff -o resource.o
//    gcc stars.c resource.o -std=c99 -Wall -Wextra -O2 $(pkg-config --cflags --libs sdl3) -o stars.exe
//
// Compile under Visual Studio Build Tools + vcpkg:
//  Install SDL3 in vcpkg:
//    vcpkg install sdl3:x64-windows
//
//  x64 Native Tools Command Prompt for VS 2022:
//    set VCPKG_ROOT=R:\VCPKG
//    rc /nologo resource.rc
//    cl /O2 /MD /I "%VCPKG_ROOT%\installed\x64-windows\include" stars.c resource.res /link /SUBSYSTEM:WINDOWS /LIBPATH:"%VCPKG_ROOT%\installed\x64-windows\lib" SDL3.lib



#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // SDL3: include explicitly for main()

#define CONFIG_FILENAME "stars.ini"
#define BETWEEN(l, u, x) (((x) < (l)) ? (l) : ((x) > (u)) ? (u) : (x))
#define MAXSTARS 500


// ------------------------- Parameters (configurable) -------------------------
static int NUM_STARS        = 100;   // number of stars (0..MAXSTARS)
static int DELAY_MS         = 100;   // delay between frames (ms)
static int BRIGHTNESS_STEP  = 15;    // brightness decrement per frame (1..255)
static int COLORED_STARS    = 1;     // 1 = random RGB, 0 = grayscale
static int BIG_STARS        = 1;     // 0=1x1, 1=2x2, 2=4x4, ...
// -----------------------------------------------------------------------------

typedef struct
{
    int x, y;
    int brightness;        // current brightness (0..255)
    int target_brightness; // always 0 in this effect
    short r, g, b;         // base color
} star_t;

static int render_w = 800;
static int render_h = 600;
static star_t stars[MAXSTARS];


// -----------------------------------------------------------------------------
// Confing file handling and INI helpers
// -----------------------------------------------------------------------------

static void trim(char *s)
{
    if (!s) return;
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, (size_t)(strlen(p) + 1));
    size_t n = strlen(s);
    while (n && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) { s[--n] = 0; }
}

static int ieq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
    {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return 0;
    }
    return *a == *b;
}

static void ini_apply_kv(const char *key, const char *val)
{
         if (ieq(key, "num_stars"))       NUM_STARS       = (int)strtol(val, NULL, 10);
    else if (ieq(key, "delay_ms"))        DELAY_MS        = (int)strtol(val, NULL, 10);
    else if (ieq(key, "brightness_step")) BRIGHTNESS_STEP = (int)strtol(val, NULL, 10);
    else if (ieq(key, "colored_stars"))   COLORED_STARS   = (int)strtol(val, NULL, 10);
    else if (ieq(key, "big_stars"))       BIG_STARS       = (int)strtol(val, NULL, 10);
}

static int CFG_Load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0; // not found
    char line[256];
    while (fgets(line, sizeof line, f))
    {
        trim(line);
        if (!line[0] || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *key = line;
        char *val = eq + 1;
        trim(key); trim(val);
        if (key[0]) ini_apply_kv(key, val);
    }
    fclose(f);
    return 1;
}

static void CFG_Check(void)
{
    NUM_STARS       = BETWEEN(0, MAXSTARS, NUM_STARS);
    DELAY_MS        = BETWEEN(0, 1000,     DELAY_MS);
    BRIGHTNESS_STEP = BETWEEN(1, 255,      BRIGHTNESS_STEP);
    COLORED_STARS   = BETWEEN(0, 1,        COLORED_STARS);
    BIG_STARS       = BETWEEN(0, 4,        BIG_STARS);
}

static int CFG_Save(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "# Number of stars displayed on the screen. (0...500)\n");
    fprintf(f, "num_stars %d\n",       NUM_STARS);
    fprintf(f, "\n# Delay between frames in milliseconds. Affects animation speed. (0...1000)\n");
    fprintf(f, "delay_ms %d\n",        DELAY_MS);
    fprintf(f, "\n# Step by which brightness decreases. Affects fading smoothness. (1...255)\n");
    fprintf(f, "brightness_step %d\n", BRIGHTNESS_STEP);
    fprintf(f, "\n# Use colored stars. (1 = yes, 0 = grayscale)\n");
    fprintf(f, "colored_stars %d\n",   COLORED_STARS);
    fprintf(f, "\n# Define star size. (0 = 1x1, 1 = 2x2, 2 = 4x4, etc., up to 4)\n");
    fprintf(f, "big_stars %d\n",       BIG_STARS);
    fclose(f);
    return 1;
}


// -----------------------------------------------------------------------------
// Renderer
// -----------------------------------------------------------------------------

static void R_RandomizeStarColor(short *r, short *g, short *b)
{
    if (COLORED_STARS)
    {
        *r = (short)(rand() % 256);
        *g = (short)(rand() % 256);
        *b = (short)(rand() % 256);
    }
    else
    {
        short gray = (short)(rand() % 256);
        *r = *g = *b = gray;
    }
}

static void R_InitStars(star_t *arr, int count, int maxx, int maxy)
{
    if (maxx <= 0 || maxy <= 0) return;

    for (int i = 0; i < count; i++)
    {
        arr[i].x = rand() % maxx;
        arr[i].y = rand() % maxy;
        arr[i].brightness = rand() % 256; // start at random intensity
        arr[i].target_brightness = 0;

        R_RandomizeStarColor(&arr[i].r, &arr[i].g, &arr[i].b);
    }
}

static void R_UpdateStarBrightness(star_t *s)
{
    if (s->brightness > s->target_brightness)
    {
        s->brightness -= BRIGHTNESS_STEP;
        if (s->brightness < s->target_brightness)
        {
            s->brightness = s->target_brightness;
        }
    }
}

static void R_UpdateStars(star_t *arr, int count, int maxx, int maxy)
{
    if (maxx <= 0 || maxy <= 0) return;

    for (int i = 0; i < count; i++)
    {
        R_UpdateStarBrightness(&arr[i]);

        if (arr[i].brightness == 0)
        {
            // respawn at new position with fresh brightness and color
            arr[i].x = rand() % maxx;
            arr[i].y = rand() % maxy;
            arr[i].brightness = 255;
            arr[i].target_brightness = 0;
            R_RandomizeStarColor(&arr[i].r, &arr[i].g, &arr[i].b);
        }
    }
}

static void R_DrawStarts(SDL_Renderer *ren, star_t *arr, int count)
{
    // Clear to black once per frame (SDL renderer is a backbuffer)
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    const int size = (BIG_STARS > 0) ? (1 << BIG_STARS) : 1;

    for (int i = 0; i < count; i++)
    {
        const int br = BETWEEN(0, 255, arr[i].brightness);

        short rr, gg, bb;
        if (COLORED_STARS)
        {
            // scale base color by brightness
            rr = (short)((arr[i].r * br) / 255);
            gg = (short)((arr[i].g * br) / 255);
            bb = (short)((arr[i].b * br) / 255);
        }
        else
        {
            rr = gg = bb = (short)br;
        }

        SDL_SetRenderDrawColor(ren, rr, gg, bb, 255);

        if (size > 1)
        {
            SDL_FRect r = { (float)arr[i].x, (float)arr[i].y, (float)size, (float)size };
            SDL_RenderFillRect(ren, &r);
        }
        else
        {
            // 1x1 "pixel"
            SDL_FRect r = { (float)arr[i].x, (float)arr[i].y, 1.0f, 1.0f };
            SDL_RenderFillRect(ren, &r);
        }
    }

    SDL_RenderPresent(ren);
}


// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------

static void I_ToggleFullScreen(SDL_Window *win, bool enable)
{
    // Toggle fullscreen and handle cursor/screensaver
    SDL_SetWindowFullscreen(win, enable); // SDL3 bool API
    if (enable)
    {
        SDL_HideCursor();
        SDL_DisableScreenSaver();
    }
    else
    {
        SDL_ShowCursor();
        SDL_EnableScreenSaver();
    }
}


// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    // (void)argc;
    // (void)argv;

    // Read config file if exist. Otherwise, create a new one with defaults.
    const int had_cfg = CFG_Load(CONFIG_FILENAME);
    
    // Check config variables.
    CFG_Check();

    // No config file? Make a new one.
    if (!had_cfg)
    CFG_Save(CONFIG_FILENAME);

    // Check for video output.
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    // Create window + renderer (let SDL pick the best driver)
    SDL_Window *win = SDL_CreateWindow("Starry Sky", 800, 600, SDL_WINDOW_RESIZABLE);
    if (!win)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, NULL);
    if (!ren)
    {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // Seed RNG and initialize stars
    srand((unsigned)time(NULL));
    SDL_GetRenderOutputSize(ren, &render_w, &render_h); // pixels
    R_InitStars(stars, NUM_STARS, render_w, render_h);

    bool running = true;
    bool is_fullscreen = false;

    while (running)
    {
        // Handle events
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            switch (ev.type)
            {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_KEY_DOWN:
                {
                    const SDL_Scancode sc = ev.key.scancode;
                    const SDL_Keymod mods = ev.key.mod;
                
                    if (sc == SDL_SCANCODE_ESCAPE)
                    {
                        running = false;
                    }
                    else if ( sc == SDL_SCANCODE_F11 || ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && (mods & SDL_KMOD_ALT)))
                    {
                        is_fullscreen = !is_fullscreen;
                        I_ToggleFullScreen(win, is_fullscreen);
                    }

                    break;
                }

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (ev.button.button == SDL_BUTTON_LEFT && ev.button.clicks >= 2)
                    {
                        is_fullscreen = !is_fullscreen;
                        I_ToggleFullScreen(win, is_fullscreen);
                    }
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                    SDL_GetRenderOutputSize(ren, &render_w, &render_h);
                    // Update imideatelly on window resize
                    R_InitStars(stars, NUM_STARS, render_w, render_h);
                    break;
            }
        }

        // Update and draw!
        R_UpdateStars(stars, NUM_STARS, render_w, render_h);
        R_DrawStarts(ren, stars, NUM_STARS);

        if (DELAY_MS > 0)
            SDL_Delay((Uint32)DELAY_MS);
    }

    // Save config file on exit
    CFG_Save(CONFIG_FILENAME);

    // Shut down SDL subsystems
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
