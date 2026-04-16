/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "ansi.h"

#define SIDE_WAVE        0
#define SIDE_MIX         1
#define SIDE_STATIC      2
#define SIDE_BREATH      3
#define SIDE_OFF         4

#define LIGHT_COLOUR_MAX 8
#define SIDE_COLOUR_MAX  8
#define LIGHT_SPEED_MAX  4
// Side-strip indicator brightness (stock value). Side LEDs are
// physically brighter than under-key LEDs, so 128 here ≈ 255 there.
#define SIDE_BLINK_LIGHT 128
#define SIDE_LINE  5
#define SIDE_INDEX 80

#define WAVE_TAB_LEN        112
#define BREATHE_TAB_LEN     128
#define FLOW_COLOUR_TAB_LEN 224

// Breath effect brightness curve (0→255→0 over 128 ticks).
static const uint8_t breathe_data_tab[BREATHE_TAB_LEN] = {
    0,   1,   2,   3,   4,   5,   6,   7,
    8,   9,   10,  12,  14,  16,  18,  20,
    22,  24,  27,  30,  33,  36,  39,  42,
    45,  49,  53,  57,  61,  65,  69,  73,
    77,  81,  85,  89,  94,  99,  104, 109,
    114, 119, 124, 129, 134, 140, 146, 152,
    158, 164, 170, 176, 182, 188, 194, 200,
    206, 213, 220, 227, 234, 241, 248, 255,
    255, 248, 241, 234, 227, 220, 213, 206,
    200, 194, 188, 182, 176, 170, 164, 158,
    152, 146, 140, 134, 129, 124, 119, 114,
    109, 104, 99,  94,  89,  85,  81,  77,
    73,  69,  65,  61,  57,  53,  49,  45,
    42,  39,  36,  33,  30,  27,  24,  22,
    20,  18,  16,  14,  12,  10,  9,   8,
    7,   6,   5,   4,   3,   2,   1,   0,
};

// Wave effect brightness curve (quick ramp, slow fall).
static const uint8_t wave_data_tab[WAVE_TAB_LEN] = {
    22,  23,  24,  25,  27,  28,  30,  31,
    33,  34,  36,  37,  39,  40,  42,  43,
    45,  47,  49,  51,  53,  55,  57,  59,
    61,  63,  65,  76,  69,  71,  73,  75,
    77,  81,  85,  89,  94,  99,  104, 109,
    114, 119, 124, 129, 134, 140, 146, 152,
    158, 164, 170, 176, 182, 188, 194, 200,
    206, 213, 220, 227, 234, 241, 248, 255,
    255, 248, 241, 234, 227, 220, 213, 206,
    200, 194, 188, 182, 176, 170, 164, 158,
    152, 146, 140, 134, 129, 124, 119, 114,
    109, 104, 99,  94,  89,  85,  81,  77,
    73,  69,  65,  61,  57,  53,  49,  45,
    42,  39,  36,  33,  30,  27,  24,  22,
};

// 224-entry rainbow: cycles through R -> Y -> G -> C -> B -> M -> W
// for the flowing side-light modes. Each row is {R, G, B}.
static const uint8_t flow_rainbow_colour_tab[FLOW_COLOUR_TAB_LEN][3] = {
    {255, 8,   8  }, {255, 8,   8  }, {255, 8,   8  }, {255, 8,   8  },
    {255, 10,  8  }, {255, 14,  8  }, {255, 18,  8  }, {255, 22,  8  },
    {255, 26,  8  }, {255, 32,  8  }, {255, 38,  8  }, {255, 44,  8  },
    {255, 50,  8  }, {255, 57,  8  }, {255, 65,  8  }, {255, 73,  8  },
    {255, 81,  8  }, {255, 89,  8  }, {255, 99,  8  }, {255, 109, 8  },
    {255, 119, 8  }, {255, 129, 8  }, {255, 140, 8  }, {255, 152, 8  },
    {255, 164, 8  }, {255, 176, 8  }, {255, 188, 8  }, {255, 200, 8  },
    {255, 213, 8  }, {255, 227, 8  }, {255, 241, 8  }, {255, 255, 8  },
    {248, 255, 8  }, {234, 255, 8  }, {220, 255, 8  }, {206, 255, 8  },
    {194, 255, 8  }, {182, 255, 8  }, {170, 255, 8  }, {158, 255, 8  },
    {146, 255, 8  }, {134, 255, 8  }, {124, 255, 8  }, {114, 255, 8  },
    {104, 255, 8  }, {94,  255, 8  }, {85,  255, 8  }, {77,  255, 8  },
    {69,  255, 8  }, {61,  255, 8  }, {53,  255, 8  }, {47,  255, 8  },
    {41,  255, 8  }, {35,  255, 8  }, {29,  255, 8  }, {24,  255, 8  },
    {20,  255, 8  }, {16,  255, 8  }, {12,  255, 8  }, {8,   255, 8  },
    {8,   255, 8  }, {8,   255, 8  }, {8,   255, 8  }, {8,   255, 8  },
    {8,   255, 8  }, {8,   255, 8  }, {8,   255, 8  }, {8,   255, 8  },
    {8,   255, 10 }, {8,   255, 14 }, {8,   255, 18 }, {8,   255, 22 },
    {8,   255, 26 }, {8,   255, 32 }, {8,   255, 38 }, {8,   255, 44 },
    {8,   255, 50 }, {8,   255, 57 }, {8,   255, 65 }, {8,   255, 73 },
    {8,   255, 81 }, {8,   255, 89 }, {8,   255, 99 }, {8,   255, 109},
    {8,   255, 119}, {8,   255, 129}, {8,   255, 140}, {8,   255, 152},
    {8,   255, 164}, {8,   255, 176}, {8,   255, 188}, {8,   255, 200},
    {8,   255, 213}, {8,   255, 227}, {8,   255, 241}, {8,   255, 255},
    {8,   248, 255}, {8,   234, 255}, {8,   220, 255}, {8,   206, 255},
    {8,   194, 255}, {8,   182, 255}, {8,   170, 255}, {8,   158, 255},
    {8,   146, 255}, {8,   134, 255}, {8,   124, 255}, {8,   114, 255},
    {8,   104, 255}, {8,   94,  255}, {8,   85,  255}, {8,   77,  255},
    {8,   69,  255}, {8,   61,  255}, {8,   53,  255}, {8,   47,  255},
    {8,   41,  255}, {8,   35,  255}, {8,   29,  255}, {8,   24,  255},
    {8,   20,  255}, {8,   16,  255}, {8,   12,  255}, {8,   8,   255},
    {8,   8,   255}, {8,   8,   255}, {8,   8,   255}, {8,   8,   255},
    {8,   8,   255}, {8,   8,   255}, {8,   8,   255}, {8,   8,   255},
    {10,  8,   255}, {14,  8,   255}, {18,  8,   255}, {22,  8,   255},
    {26,  8,   255}, {32,  8,   255}, {38,  8,   255}, {44,  8,   255},
    {50,  8,   255}, {57,  8,   255}, {65,  8,   255}, {73,  8,   255},
    {81,  8,   255}, {89,  8,   255}, {99,  8,   255}, {109, 8,   255},
    {119, 8,   255}, {129, 8,   255}, {140, 8,   255}, {152, 8,   255},
    {164, 8,   255}, {176, 8,   255}, {188, 8,   255}, {200, 8,   255},
    {213, 8,   255}, {227, 8,   255}, {241, 8,   255}, {255, 8,   255},
    {255, 8,   255}, {255, 8,   255}, {255, 8,   255}, {255, 8,   255},
    {255, 10,  255}, {255, 14,  255}, {255, 18,  255}, {255, 22,  255},
    {255, 26,  255}, {255, 32,  255}, {255, 38,  255}, {255, 44,  255},
    {255, 50,  255}, {255, 57,  255}, {255, 65,  255}, {255, 73,  255},
    {255, 81,  255}, {255, 89,  255}, {255, 99,  255}, {255, 109, 255},
    {255, 119, 255}, {255, 129, 255}, {255, 140, 255}, {255, 152, 255},
    {255, 164, 255}, {255, 176, 255}, {255, 188, 255}, {255, 200, 255},
    {255, 213, 255}, {255, 227, 255}, {255, 241, 255}, {255, 255, 255},
    {255, 248, 248}, {255, 234, 234}, {255, 220, 220}, {255, 206, 206},
    {255, 194, 194}, {255, 182, 182}, {255, 170, 170}, {255, 158, 158},
    {255, 146, 146}, {255, 134, 134}, {255, 124, 124}, {255, 114, 114},
    {255, 104, 104}, {255, 94,  94 }, {255, 85,  85 }, {255, 77,  77 },
    {255, 69,  69 }, {255, 61,  61 }, {255, 53,  53 }, {255, 47,  47 },
    {255, 41,  41 }, {255, 35,  35 }, {255, 29,  29 }, {255, 24,  24 },
    {255, 20,  20 }, {255, 16,  16 }, {255, 12,  12 }, {255, 8,   8  },
    {255, 8,   8  }, {255, 8,   8  }, {255, 8,   8  }, {255, 8,   8  },
};

// Side-light colour palette (index set by SIDE_HUI keycode).
static const uint8_t colour_lib[9][3] = {
    {0xff, 0x00, 0x00}, // red
    {0xff, 0x40, 0x00}, // orange
    {0xff, 0xff, 0x00}, // yellow
    {0x00, 0xff, 0x00}, // green
    {0x00, 0xff, 0xff}, // cyan
    {0x00, 0x00, 0xff}, // blue
    {0x80, 0x00, 0xff}, // purple
    {0xc0, 0xc0, 0xff}, // pale blue
    {0x00, 0x00, 0x00}, // off
};

const uint8_t side_speed_table[5][5] = {
    [SIDE_WAVE]   = {20, 28, 36, 44, 52},
    [SIDE_MIX]    = {46, 54, 62, 70, 78},
    [SIDE_STATIC] = {50, 50, 50, 50, 50},
    [SIDE_BREATH] = {30, 38, 46, 54, 62},
    [SIDE_OFF]    = {50, 50, 50, 50, 50},
};

const uint8_t side_light_table[6] = {
    0,
    60,
    120,
    180,
    255,
};

const uint8_t side_led_index_tab[SIDE_LINE] =
    {
        SIDE_INDEX + 4,
        SIDE_INDEX + 3,
        SIDE_INDEX + 2,
        SIDE_INDEX + 1,
        SIDE_INDEX + 0,
};

uint8_t side_mode           = 0;
uint8_t side_light          = 2;
uint8_t side_speed          = 2;
uint8_t side_rgb            = 1;
uint8_t side_colour         = 0;
uint8_t side_play_point     = 0;
uint8_t side_play_cnt       = 0;
uint32_t side_play_timer    = 0;
uint8_t r_temp, g_temp, b_temp;

extern dev_info_t dev_info;
extern bool f_bat_hold;
extern user_config_t user_config;
extern uint8_t rf_blink_cnt;
extern uint16_t rf_link_show_time;

void suspend_power_down_kb(void)
{
    rgb_matrix_set_suspend_state(true);
}

void suspend_wakeup_init_kb(void)
{
    rgb_matrix_set_suspend_state(false);
}

// Adjusts side-light brightness. brighten: 1 = up, 0 = down.
void light_level_control(uint8_t brighten)
{
    if (brighten)
    {
        if (side_light == 4) {
            return;
        } else
            side_light++;
    } else
    {
        if (side_light == 0) {
            return;
        } else
            side_light--;
    }
    user_config.ee_side_light = side_light;
    user_config_schedule_save();
}

// Adjusts side-light animation speed. fast: 1 = faster, 0 = slower.
void light_speed_control(uint8_t fast)
{
    if ((side_speed) > LIGHT_SPEED_MAX)
        (side_speed) = LIGHT_SPEED_MAX / 2;

    if (fast) {
        if ((side_speed)) side_speed--;
    } else {
        if ((side_speed) < LIGHT_SPEED_MAX) side_speed++;
    }
    user_config.ee_side_speed = side_speed;
    user_config_schedule_save();
}

// Cycles through side-light colors. dir: 1 = next, 0 = prev.
void side_colour_control(uint8_t dir)
{
    if (side_mode != SIDE_WAVE) {
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = 0;
        }
    }

    if (dir) {
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = 0;
        } else {
            side_colour++;
            if (side_colour >= LIGHT_COLOUR_MAX) {
                side_rgb    = 1;
                side_colour = 0;
            }
        }
    } else {
        if (side_rgb) {
            side_rgb    = 0;
            side_colour = LIGHT_COLOUR_MAX - 1;
        } else {
            side_colour--;
            if (side_colour >= LIGHT_COLOUR_MAX) {
                side_rgb    = 1;
                side_colour = 0;
            }
        }
    }
    user_config.ee_side_rgb    = side_rgb;
    user_config.ee_side_colour = side_colour;
    user_config_schedule_save();
}

// Cycles through side-light animation modes. dir: 1 = next, 0 = prev.
void side_mode_control(uint8_t dir)
{
    if (dir) {
        side_mode++;
        if (side_mode > SIDE_OFF) {
            side_mode = 0;
        }
    } else {
        if (side_mode > 0) {
            side_mode--;
        } else {
            side_mode = SIDE_OFF;
        }
    }
    side_play_point          = 0;
    user_config.ee_side_mode = side_mode;
    user_config_schedule_save();
}

void set_left_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    for (int i = 0; i < SIDE_LINE; i++)
        rgb_matrix_set_color(SIDE_INDEX + i, r, g, b);
}

// Generic side-strip 3-blink feedback. Call side_flash_trigger() from
// anywhere to flash the left strip in the given colour 3 times (~3s).
// Replaces the old per-feature flash functions with one reusable path.
static struct {
    uint8_t  r, g, b;
    uint32_t timer;
    bool     active;
} side_flash;

void side_flash_trigger(uint8_t r, uint8_t g, uint8_t b) {
    side_flash.r      = r;
    side_flash.g      = g;
    side_flash.b      = b;
    side_flash.timer  = timer_read32();
    side_flash.active = true;
}

static void side_flash_show(void) {
    // Continuous red overlay while BOOT_HOLD is active, synced with
    // the Esc key blink rate (~3 Hz / 166ms half-period).
    extern bool boot_hold_active;
    if (boot_hold_active) {
        if ((timer_read() / 166) % 2) {
            set_left_rgb(SIDE_BLINK_LIGHT, 0, 0);
        } else {
            set_left_rgb(0, 0, 0);
        }
        return;  // boot-hold takes priority over toggle flashes
    }

    // One-shot 3-blink triggered by toggles (win-lock, jiggler, etc.).
    // Pattern: ON 500ms, OFF 500ms × 3 = 3000ms total.  Timeout is
    // checked first so we never draw a partial 4th flash.
    if (!side_flash.active) return;
    if (timer_elapsed32(side_flash.timer) >= 3000) {
        side_flash.active = false;
        return;
    }
    if ((timer_elapsed32(side_flash.timer) / 500) % 2 == 0) {
        set_left_rgb(side_flash.r, side_flash.g, side_flash.b);
    } else {
        set_left_rgb(0, 0, 0);
    }
}

// OS mode switch feedback — white for Mac, blue for Win.
void sys_sw_led_show(void)
{
    extern bool f_sys_show;
    if (f_sys_show) {
        f_sys_show = false;
        if (dev_info.sys_sw_state == SYS_SW_MAC)
            side_flash_trigger(SIDE_BLINK_LIGHT, SIDE_BLINK_LIGHT, SIDE_BLINK_LIGHT);
        else
            side_flash_trigger(0, 0, SIDE_BLINK_LIGHT);
    }
}

// Sleep toggle feedback — green for enabled, red for disabled.
void sleep_sw_led_show(void)
{
    extern bool f_sleep_show;
    if (f_sleep_show) {
        f_sleep_show = false;
        if (user_config.sleep_enable)
            side_flash_trigger(0, SIDE_BLINK_LIGHT, 0);
        else
            side_flash_trigger(SIDE_BLINK_LIGHT, 0, 0);
    }
}

// Caps Lock → cyan on side strip.
void sys_led_show(void)
{
    if (dev_info.link_mode == LINK_USB) {
        if (host_keyboard_led_state().caps_lock) {
            set_left_rgb(0X00, SIDE_BLINK_LIGHT, SIDE_BLINK_LIGHT);
        }
    }
    else {
        if (dev_info.rf_led & 0x02) {
            set_left_rgb(0X00, SIDE_BLINK_LIGHT, SIDE_BLINK_LIGHT);
        }
    }
}

// Advances an animation cursor (*point) by `step` within a cyclic
// table of length `len`. trend: 1 = forward, 0 = backward.
static void light_point_playing(uint8_t trend, uint8_t step, uint8_t len, uint8_t *point)
{
    if (trend) {
        *point += step;
        if (*point >= len) *point -= len;
    } else {
        *point -= step;
        if (*point >= len) *point = len - (255 - *point) - 1;
    }
}

// Scales r_temp/g_temp/b_temp by (light_temp/255). Used to apply a
// brightness curve point to the current side-light color.
static void count_rgb_light(uint8_t light_temp)
{
    uint16_t temp;

    temp   = (light_temp)*r_temp + r_temp;
    r_temp = temp >> 8;

    temp   = (light_temp)*g_temp + g_temp;
    g_temp = temp >> 8;

    temp   = (light_temp)*b_temp + b_temp;
    b_temp = temp >> 8;
}

static void side_wave_mode_show(void)
{
    uint8_t play_index;

    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    if (side_rgb)
        light_point_playing(0, 1, FLOW_COLOUR_TAB_LEN, &side_play_point);
    else
        light_point_playing(0, 2, WAVE_TAB_LEN, &side_play_point);

    play_index = side_play_point;
    for (int i = 0; i < SIDE_LINE; i++) {
        if (side_rgb) {
            r_temp = flow_rainbow_colour_tab[play_index][0];
            g_temp = flow_rainbow_colour_tab[play_index][1] * 0.3;
            b_temp = flow_rainbow_colour_tab[play_index][2] * 0.4;

            light_point_playing(1, 3, FLOW_COLOUR_TAB_LEN, &play_index);
        } else {
            r_temp = colour_lib[side_colour][0];
            g_temp = colour_lib[side_colour][1];
            b_temp = colour_lib[side_colour][2];

            light_point_playing(1, 12, WAVE_TAB_LEN, &play_index);
            count_rgb_light(wave_data_tab[play_index]);
        }

        count_rgb_light(side_light_table[side_light]);

        rgb_matrix_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

static void side_spectrum_mode_show(void)
{
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    light_point_playing(1, 1, FLOW_COLOUR_TAB_LEN, &side_play_point);

    r_temp = flow_rainbow_colour_tab[side_play_point][0];
    g_temp = flow_rainbow_colour_tab[side_play_point][1];
    b_temp = flow_rainbow_colour_tab[side_play_point][2];

    count_rgb_light(side_light_table[side_light]);

    for (int i = 0; i < SIDE_LINE; i++) {
        rgb_matrix_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

static void side_breathe_mode_show(void)
{
    static uint8_t play_point = 0;

    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    light_point_playing(0, 1, BREATHE_TAB_LEN, &play_point);

    r_temp = colour_lib[side_colour][0];
    g_temp = colour_lib[side_colour][1];
    b_temp = colour_lib[side_colour][2];

    count_rgb_light(breathe_data_tab[play_point]);
    count_rgb_light(side_light_table[side_light]);

    for (int i = 0; i < SIDE_LINE; i++) {
        rgb_matrix_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

static void side_static_mode_show(void)
{
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    if (side_play_point >= SIDE_COLOUR_MAX) side_play_point = 0;

    for (int i = 0; i < SIDE_LINE; i++) {
        r_temp = colour_lib[side_colour][0];
        g_temp = colour_lib[side_colour][1];
        b_temp = colour_lib[side_colour][2];

        count_rgb_light(side_light_table[side_light]);

        rgb_matrix_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

static void side_off_mode_show(void)
{
    if (side_play_cnt <= side_speed_table[side_mode][side_speed])
        return;
    else
        side_play_cnt -= side_speed_table[side_mode][side_speed];
    if (side_play_cnt > 20) side_play_cnt = 0;

    r_temp = 0x00;
    g_temp = 0x00;
    b_temp = 0x00;

    for (int i = 0; i < SIDE_LINE; i++) {
        rgb_matrix_set_color(side_led_index_tab[i], r_temp, g_temp, b_temp);
    }
}

#define RF_LED_LINK_PERIOD 500
#define RF_LED_PAIR_PERIOD 250

// Drives the left side strip to show RF link state: solid color when
// connected, blinking while pairing/linking, off once stable.
void rf_led_show(void)
{
    static uint32_t rf_blink_timer = 0;
    uint16_t rf_blink_priod        = 0;
    static bool flag_power_on = 1;

    if (dev_info.link_mode == LINK_RF_24)
    {
        r_temp = 0x00;
        g_temp = SIDE_BLINK_LIGHT;
        b_temp = 0x00;
    } else if (dev_info.link_mode == LINK_USB) {
        r_temp = SIDE_BLINK_LIGHT;
        g_temp = SIDE_BLINK_LIGHT;
        b_temp = 0x00;
        if (flag_power_on && (rf_link_show_time < RF_LINK_SHOW_TIME)) return;
    } else
    {
        r_temp = 0x00;
        g_temp = 0x00;
        b_temp = SIDE_BLINK_LIGHT;
    }

    flag_power_on = 0;

    if (rf_blink_cnt)
    {
        if (dev_info.rf_state == RF_PAIRING)
            rf_blink_priod = RF_LED_PAIR_PERIOD;
        else
            rf_blink_priod = RF_LED_LINK_PERIOD;

        if (timer_elapsed32(rf_blink_timer) < (rf_blink_priod >> 1)) {
        } else {
            r_temp = 0x00;
            g_temp = 0x00;
            b_temp = 0x00;
        }

        if (timer_elapsed32(rf_blink_timer) >= rf_blink_priod) {
            rf_blink_cnt--;
            rf_blink_timer = timer_read32();
        }
    } else if (rf_link_show_time < RF_LINK_SHOW_TIME) {
    } else {
        rf_blink_timer = timer_read32();
        return;
    }

    set_left_rgb(r_temp, g_temp, b_temp);
}

uint8_t bat_pwm_buf[6 * 3] = {0};
uint8_t bat_end_led        = 0;
uint8_t bat_r, bat_g, bat_b;

// Maps battery percentage to a color on the left side strip:
// <=20% red, <=80% amber, >80% green.
void bat_percent_led(uint8_t bat_percent)
{
    if (bat_percent <= 20) {
        bat_end_led = 0;
        bat_r = SIDE_BLINK_LIGHT, bat_g = 0, bat_b = 0;
    } else if (bat_percent <= 30) {
        bat_end_led = 1;
        bat_r = SIDE_BLINK_LIGHT, bat_g = SIDE_BLINK_LIGHT / 2, bat_b = 0;
    } else if (bat_percent <= 50) {
        bat_end_led = 2;
        bat_r = SIDE_BLINK_LIGHT, bat_g = SIDE_BLINK_LIGHT / 2, bat_b = 0;
    } else if (bat_percent <= 80) {
        bat_end_led = 3;
        bat_r = SIDE_BLINK_LIGHT, bat_g = SIDE_BLINK_LIGHT / 2, bat_b = 0;
    } else {
        bat_end_led = 4;
        bat_r = 0, bat_g = SIDE_BLINK_LIGHT, bat_b = 0;
    }

    uint8_t i;
    for (i = 0; i < SIDE_LINE; i++)
    rgb_matrix_set_color(SIDE_INDEX + i, bat_r, bat_g, bat_b);
}

// Shows battery status on the left side strip: briefly when charging
// state changes or battery drops below 10%, held while BAT_SHOW is
// toggled on.
void bat_led_show(void)
{
    static uint8_t play_point      = 0;
    static uint32_t bat_play_timer = 0;

    static uint32_t bat_show_time = 0;
    static bool bat_show_flag     = true;
    static bool bat_show_breath   = false;
    static uint32_t bat_sts_debounce = 0;
    static uint32_t bat_per_debounce = 0;
    static uint8_t charge_state      = 0;
    static uint8_t bat_percent       = 0;
    static bool f_init               = 1;

    if (f_init) {
        f_init        = 0;
        bat_show_time = timer_read32();
        charge_state  = dev_info.rf_charge;
        bat_percent   = dev_info.rf_battery;
    }

    if (charge_state != dev_info.rf_charge) {
        if (timer_elapsed32(bat_sts_debounce) > 1000) {
            if (((charge_state & 0x01) == 0) && ((dev_info.rf_charge & 0x01) != 0)) {
                bat_show_flag   = true;
                bat_show_breath = true;
                bat_show_time   = timer_read32();
            }
            charge_state = dev_info.rf_charge;
        }
    }
    else {
        bat_sts_debounce = timer_read32();
        if (timer_elapsed32(bat_show_time) > 5000) {
            bat_show_flag   = false;
            bat_show_breath = false;
        }
        if (charge_state == 0x03) {
            bat_show_breath = true;
        }
        else if (charge_state & 0x01) {
            dev_info.rf_battery = 100;
        }
    }

    if (bat_percent != dev_info.rf_battery) {
        if (timer_elapsed32(bat_per_debounce) > 1000) {
            bat_percent = dev_info.rf_battery;
        }
    }
    else {
        bat_per_debounce = timer_read32();
        if (bat_percent < 10) {
            bat_show_flag = true;
            bat_show_time = timer_read32();
        }
    }

    if (f_bat_hold || bat_show_flag) {
        if (bat_show_breath) {
            if (timer_elapsed32(bat_play_timer) > 10) {
                bat_play_timer = timer_read32();
                light_point_playing(0, 1, BREATHE_TAB_LEN, &play_point);
            }
            r_temp = 0xff;
            g_temp = 0x40;
            b_temp = 0x00;
            count_rgb_light(breathe_data_tab[play_point]);
            set_left_rgb(r_temp, g_temp, b_temp);
        }
        else {
            bat_percent_led(bat_percent);
        }
    }
}


// Plays a 3x white-flash animation as feedback after DEV_RESET fires.
void device_reset_show(void)
{
    gpio_write_pin_high(DC_BOOST_PIN);
    gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_write_pin_high(RGB_DRIVER_SDB2);
    for (int blink_cnt = 0; blink_cnt < 3; blink_cnt++) {
        rgb_matrix_set_color_all(0xFF, 0xFF, 0xFF);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);

        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);
    }
}

// Resets side-light / RGB matrix state to factory defaults and
// schedules the new config to be written to EEPROM.
void device_reset_init(void)
{
    side_mode       = 0;
    side_light      = 3;
    side_speed      = 2;
    side_rgb        = 1;
    side_colour     = 0;
    side_play_point = 0;

    side_play_cnt   = 0;
    side_play_timer = timer_read32();

    f_bat_hold = false;

    rgb_matrix_enable();
    rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
    rgb_matrix_set_speed(255 - RGB_MATRIX_SPD_STEP * 2);
    rgb_matrix_sethsv(255, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2);

    user_config.default_brightness_flag = 0xA5;
    user_config.ee_side_mode            = side_mode;
    user_config.ee_side_light           = side_light;
    user_config.ee_side_speed           = side_speed;
    user_config.ee_side_rgb             = side_rgb;
    user_config.ee_side_colour          = side_colour;
    user_config.sleep_enable            = true;
    user_config_schedule_save();
}

// Flashes the whole matrix through red/green/blue (1s each) as a
// manual LED test. Blocks for ~3 seconds.
void rgb_test_show(void)
{
    gpio_write_pin_high(DC_BOOST_PIN);
    gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_write_pin_high(RGB_DRIVER_SDB2);

    rgb_matrix_set_color_all(0xFF, 0x00, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);

    rgb_matrix_set_color_all(0x00, 0xFF, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);

    rgb_matrix_set_color_all(0x00, 0x00, 0xFF);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);
}

// Top-level side-strip driver, called from housekeeping each tick.
// Runs the active animation then overlays indicators (battery, caps,
// OS switch, sleep, RF link) on top.
void m_side_led_show(void)
{
    static bool flag_power_on         = 1;
    extern bool f_dial_sw_init_ok;
    side_play_cnt += timer_elapsed32(side_play_timer);
    side_play_timer = timer_read32();

    if (flag_power_on) {
        if (!f_dial_sw_init_ok) return;
        flag_power_on = 0;
    }

    switch (side_mode) {
        case SIDE_WAVE:     side_wave_mode_show();      break;
        case SIDE_MIX:      side_spectrum_mode_show();  break;
        case SIDE_BREATH:   side_breathe_mode_show();   break;
        case SIDE_STATIC:   side_static_mode_show();    break;
        case SIDE_OFF:      side_off_mode_show();       break;
    }

    bat_led_show();
    sys_led_show();
    sys_sw_led_show();
    sleep_sw_led_show();
    side_flash_show();
    rf_led_show();
}