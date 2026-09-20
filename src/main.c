#include <SDL2/SDL.h>
#include "lvgl/lvgl.h"
#include "keyboard.h" // 引入你的键盘模块

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 720

static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture *sdl_texture = NULL;

static void mouse_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    int x = 0, y = 0;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    data->point.x = (int32_t)x;
    data->point.y = (int32_t)y;
    data->state = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    SDL_UpdateTexture(sdl_texture, NULL, px_map, SCREEN_WIDTH * 4);
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
    lv_disp_flush_ready(disp);
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    SDL_Window* window = SDL_CreateWindow("61-Key Bubble FX",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    lv_init();
    
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    static uint32_t buf_1[SCREEN_WIDTH * SCREEN_HEIGHT];
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, disp_flush);

    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, mouse_read_cb);
    lv_indev_set_display(indev, disp);

    // 调用 keyboard.c 里的初始化函数，生成界面
    create_keyboard_ui();

    int running = 1;
    SDL_Event event;
    uint32_t last_tick = SDL_GetTicks();
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        
        uint32_t current_tick = SDL_GetTicks();
        lv_tick_inc(current_tick - last_tick);
        last_tick = current_tick;

        lv_timer_handler(); 
        SDL_Delay(5);      
    }

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}