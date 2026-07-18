#include "SDL_Driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "SDL.h"
#include "../main/display/ui_manager.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;

static lv_color_t *buf1 = NULL;

static bool needs_present = false;

static uint8_t circle_mask[SIM_LCD_V_RES][SIM_LCD_H_RES];

static void generate_circle_mask(void)
{
    float cx = SIM_LCD_H_RES / 2.0f;
    float cy = SIM_LCD_V_RES / 2.0f;
    float radius = SIM_LCD_H_RES / 2.0f - 2.0f;
    float edge = 3.0f;

    for (int y = 0; y < SIM_LCD_V_RES; y++) {
        for (int x = 0; x < SIM_LCD_H_RES; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist <= radius - edge) {
                circle_mask[y][x] = 255;
            } else if (dist >= radius + edge) {
                circle_mask[y][x] = 0;
            } else {
                float t = 1.0f - (dist - (radius - edge)) / (2.0f * edge);
                circle_mask[y][x] = (uint8_t)(t * 255);
            }
        }
    }
}

static void sdl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;
    int w = x2 - x1;
    int h = y2 - y1;

    lv_color_t *src = color_map;
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            if (circle_mask[y][x] == 0) {
                src[(y - y1) * SIM_LCD_H_RES + (x - x1)] = lv_color_make(0, 0, 0);
            }
        }
    }

    void *pixels;
    int pitch;
    SDL_Rect rect = { x1, y1, w, h };

    if (SDL_LockTexture(texture, &rect, &pixels, &pitch) == 0) {
        uint16_t *sp = (uint16_t *)color_map;
        uint16_t *dp = (uint16_t *)pixels;
        for (int row = 0; row < h; row++) {
            memcpy(dp, sp, w * sizeof(lv_color_t));
            sp += SIM_LCD_H_RES;
            dp += pitch / sizeof(lv_color_t);
        }
        SDL_UnlockTexture(texture);
    }

    SDL_RenderCopy(renderer, texture, &rect, &rect);
    needs_present = true;

    lv_disp_flush_ready(drv);
}

void sdl_driver_present(void)
{
    if (needs_present) {
        SDL_RenderPresent(renderer);
        needs_present = false;
    }
}

static int enc_diff = 0;
static bool enc_pressed = false;
static bool enc_released = false;
static bool button_is_down = false;

static void sdl_encoder_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:
                        enc_diff--;
                        break;
                    case SDLK_RIGHT:
                        enc_diff++;
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (!button_is_down) {
                            enc_pressed = true;
                            button_is_down = true;
                        }
                        break;
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        exit(0);
                        break;
                    default:
                        break;
                }
                break;
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        enc_released = true;
                        button_is_down = false;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_QUIT:
                exit(0);
                break;
            default:
                break;
        }
    }

    data->enc_diff = enc_diff;
    enc_diff = 0;

    if (enc_pressed) {
        data->state = LV_INDEV_STATE_PR;
        enc_pressed = false;
    } else if (enc_released) {
        data->state = LV_INDEV_STATE_REL;
        enc_released = false;
    } else {
        data->state = button_is_down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    }

    ui_manager_encoder_event(data);
}

void sdl_driver_init(void)
{
    printf("[SDL] Initializing SDL2 window (%dx%d)...\n", SIM_LCD_H_RES, SIM_LCD_V_RES);

    generate_circle_mask();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }

    window = SDL_CreateWindow(
        "StudyBud Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SIM_LCD_H_RES, SIM_LCD_V_RES,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        exit(1);
    }

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        SIM_LCD_H_RES, SIM_LCD_V_RES
    );
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        exit(1);
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    buf1 = (lv_color_t *)malloc(SIM_LCD_H_RES * SIM_LCD_V_RES * sizeof(lv_color_t));
    assert(buf1);

    lv_disp_draw_buf_init(&disp_buf, buf1, NULL, SIM_LCD_H_RES * SIM_LCD_V_RES);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SIM_LCD_H_RES;
    disp_drv.ver_res = SIM_LCD_V_RES;
    disp_drv.flush_cb = sdl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.full_refresh = 1;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.disp = disp;
    indev_drv.read_cb = sdl_encoder_read;
    lv_indev_drv_register(&indev_drv);

    printf("[SDL] SDL2 driver initialized successfully\n");
}

void sdl_driver_loop(void)
{
    lv_timer_handler();
    sdl_driver_present();
}
