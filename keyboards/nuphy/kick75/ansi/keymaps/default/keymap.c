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

#include QMK_KEYBOARD_H

extern dev_info_t dev_info;

enum keymap_keycodes {
    BOOT_HOLD = QK_USER_0,
    ENC_TAB_FWD,   // Fn+encoder CW  → Alt/Cmd-Tab forward
    ENC_TAB_REV,   // Fn+encoder CCW → Alt/Cmd-Shift-Tab backward
};

#define BOOT_HOLD_MS    1660  // ~5 red blinks at 3 Hz
#define ALT_TAB_TIMEOUT 500   // ms before auto-selecting the window

// Shared with ansi.c for the red Esc blink indicator.
bool            boot_hold_active = false;
static uint16_t boot_hold_timer  = 0;

// Alt-Tab window switcher state. While active, the modifier key
// (Cmd on Mac, Alt on Win) stays registered. Each encoder detent
// taps Tab (or Shift-Tab). After ALT_TAB_TIMEOUT of no rotation,
// the modifier is released and the highlighted window is selected.
static bool     alt_tab_active = false;
static uint32_t alt_tab_timer  = 0;

static uint8_t alt_tab_mod(void) {
    return (dev_info.sys_sw_state == SYS_SW_MAC) ? KC_LGUI : KC_LALT;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (keycode == BOOT_HOLD) {
        if (record->event.pressed) {
            boot_hold_timer  = timer_read();
            boot_hold_active = true;
        } else {
            boot_hold_active = false;
            if (timer_elapsed(boot_hold_timer) > BOOT_HOLD_MS) {
                reset_keyboard();
            }
        }
        return false;
    }
    if ((keycode == ENC_TAB_FWD || keycode == ENC_TAB_REV) && record->event.pressed) {
        if (!alt_tab_active) {
            alt_tab_active = true;
            register_code(alt_tab_mod());
        }
        if (keycode == ENC_TAB_REV) register_code(KC_LSFT);
        tap_code(KC_TAB);
        if (keycode == ENC_TAB_REV) unregister_code(KC_LSFT);
        alt_tab_timer = timer_read32();
        return false;
    }
    return true;
}

void housekeeping_task_user(void) {
    if (alt_tab_active && timer_elapsed32(alt_tab_timer) > ALT_TAB_TIMEOUT) {
        unregister_code(alt_tab_mod());
        alt_tab_active = false;
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

// layer Mac
[0] = LAYOUT(
    KC_ESC, 	KC_BRID,  	KC_BRIU,  	MAC_TASK, 	MAC_SEARCH, MAC_VOICE,  MAC_DND,  	KC_MPRV,  	KC_MPLY,  	KC_MNXT, 	KC_MUTE, 	KC_VOLD, 	KC_VOLU, 	KC_DEL, 	KC_MUTE,
	KC_GRV, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 	KC_BSPC,	KC_HOME,
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC,	KC_BSLS,	KC_PGUP,
	KC_CAPS,	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	KC_ENT,                 KC_PGDN,
	KC_LSFT,				KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,	KC_RSFT,	KC_UP,
	KC_LCTL,	KC_LOPT,	KC_LCMD,										KC_SPC, 							KC_RCMD,	MO(1),   				KC_LEFT,	KC_DOWN,    KC_RIGHT),
// layer Mac Fn
[1] = LAYOUT(
	BOOT_HOLD, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	KC_INS,	    _______,
	_______, 	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	KC_END,
	RM_TOGG, 	_______,   	_______,   	_______,   	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, BAT_SHOW,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	_______,                _______,
	_______,				_______,   	_______,   	_______,  	_______,    _______,   	_______,	MO(4), 		RM_SPDD,	RM_SPDU,	_______,	_______,	RM_VALU,
	MOUSE_JIGGLE,_______,	WIN_LOCK,									_______, 							_______,	MO(1),		            RM_NEXT,    RM_VALD,	RM_HUEU),
// layer win
[2] = LAYOUT(
	KC_ESC, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	KC_DEL,    KC_MUTE,
	KC_GRV, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 	KC_BSPC,   KC_HOME,	    
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC,	KC_BSLS,   KC_PGUP,	
	KC_CAPS,	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	KC_ENT, 	           KC_PGDN,
	KC_LSFT,				KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,	KC_RSFT,    KC_UP,						
	KC_LCTL,	KC_LWIN,	KC_LALT,										KC_SPC, 							KC_RALT,	MO(3),	                KC_LEFT,   	KC_DOWN,	KC_RIGHT),
// layer win Fn
[3] = LAYOUT(
	BOOT_HOLD, 	KC_BRID,  	KC_BRIU,  	_______, 	_______,  	_______,  	_______,  	KC_MPRV,  	KC_MPLY,  	KC_MNXT, 	KC_MUTE, 	KC_VOLD, 	KC_VOLU, 	KC_INS,	    _______,
	_______, 	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	KC_END,
	RM_TOGG,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, BAT_SHOW,	_______,	
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	_______,                _______,
	_______,				_______,   	_______,   	_______,  	_______,   	_______,   	_______,	MO(4), 		RM_SPDD,	RM_SPDU,	_______,	_______,	RM_VALU,
	MOUSE_JIGGLE,WIN_LOCK,	_______,									_______, 							_______,	MO(3),					RM_NEXT,    RM_VALD,	RM_HUEU),
// layer side led fn+m
[4] = LAYOUT(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,
	_______, 	_______,  	_______,  	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	_______,                _______,
	_______,				_______,   	_______,   	RGB_TEST,  	_______,   	_______,   	_______,	_______, 	SIDE_SPD,	SIDE_SPI,	_______,	_______,	SIDE_VAI,
	_______,	_______,	_______,										_______, 							_______,	MO(4),   	        	SIDE_MOD,   SIDE_VAD,	SIDE_HUI),
// layer reserved
[5] = LAYOUT(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,	
	_______, 	_______,  	_______,  	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,	
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	_______,                _______,
	_______,				_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______, 	_______,	_______,	_______,	_______,	_______,
	_______,	_______,	_______,										_______, 							_______,	_______,			    _______,    _______,    _______),
// layer reserved
[6] = LAYOUT(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,	
	_______, 	_______,  	_______,  	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______, 	_______,	_______,	
	_______,	_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______,   	_______,   	_______,  	_______,	_______, 	_______,                _______,
	_______,				_______,   	_______,   	_______,  	_______,   	_______,   	_______,	_______, 	_______,	_______,	_______,	_______,	_______,
	_______,	_______,	_______,										_______, 							_______,	_______,			    _______,    _______,    _______),
[7] = LAYOUT(
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    
    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,                _______,
    _______,                _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,    _______,
    _______,    _______,    _______,                                        _______,                            _______,    _______,                _______,    _______,    _______),
};


#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][NUM_DIRECTIONS] = {
    [0] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [1] = { ENCODER_CCW_CW(ENC_TAB_REV, ENC_TAB_FWD) },  // Fn: window switch
    [2] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [3] = { ENCODER_CCW_CW(ENC_TAB_REV, ENC_TAB_FWD) },  // Fn: window switch
    [4] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [5] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [6] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
    [7] = { ENCODER_CCW_CW(KC_VOLD, KC_VOLU) },
};
#endif

const is31fl3733_led_t PROGMEM g_is31fl3733_leds[IS31FL3733_LED_COUNT] = {
    {0, SW1_CS12,   SW2_CS12,   SW3_CS12},   
    {0, SW1_CS11,   SW2_CS11,   SW3_CS11},   
    {0, SW1_CS10,   SW2_CS10,   SW3_CS10},   
    {0, SW1_CS9,    SW2_CS9,    SW3_CS9},    
    {0, SW4_CS12,   SW5_CS12,   SW6_CS12},   
    {0, SW4_CS11,   SW5_CS11,   SW6_CS11},   
    {0, SW4_CS10,   SW5_CS10,   SW6_CS10},   
    {0, SW4_CS9,    SW5_CS9,    SW6_CS9},    
    {1, SW4_CS13,   SW5_CS13,   SW6_CS13},   
    {1, SW4_CS12,   SW5_CS12,   SW6_CS12},   
    {1, SW4_CS11,   SW5_CS11,   SW6_CS11},   
    {1, SW4_CS10,   SW5_CS10,   SW6_CS10},   
    {1, SW7_CS13,   SW8_CS13,   SW9_CS13},   
    {1, SW10_CS7,    SW11_CS7,    SW12_CS7},    
    {0, SW1_CS1,    SW2_CS1,    SW3_CS1},    
    {0, SW1_CS2,    SW2_CS2,    SW3_CS2},    
    {0, SW1_CS3,    SW2_CS3,    SW3_CS3},    
    {0, SW1_CS4,    SW2_CS4,    SW3_CS4},    
    {0, SW1_CS5,    SW2_CS5,    SW3_CS5},    
    {0, SW1_CS6,    SW2_CS6,    SW3_CS6},    
    {0, SW1_CS7,    SW2_CS7,    SW3_CS7},    
    {0, SW1_CS8,    SW2_CS8,    SW3_CS8},    
    {1, SW4_CS1,    SW5_CS1,    SW6_CS1},    
    {1, SW4_CS2,    SW5_CS2,    SW6_CS2},    
    {1, SW4_CS3,    SW5_CS3,    SW6_CS3},    
    {1, SW4_CS4,    SW5_CS4,    SW6_CS4},    
    {1, SW4_CS5,    SW5_CS5,    SW6_CS5},    
    {1, SW4_CS6,    SW5_CS6,    SW6_CS6},    
    {1, SW10_CS8,    SW11_CS8,    SW12_CS8},    
    {0, SW4_CS1,    SW5_CS1,    SW6_CS1},    
    {0, SW4_CS2,    SW5_CS2,    SW6_CS2},    
    {0, SW4_CS3,    SW5_CS3,    SW6_CS3},    
    {0, SW4_CS4,    SW5_CS4,    SW6_CS4},    
    {0, SW4_CS5,    SW5_CS5,    SW6_CS5},    
    {0, SW4_CS6,    SW5_CS6,    SW6_CS6},    
    {0, SW4_CS7,    SW5_CS7,    SW6_CS7},    
    {0, SW4_CS8,    SW5_CS8,    SW6_CS8},    
    {1, SW7_CS1,    SW8_CS1,    SW9_CS1},    
    {1, SW7_CS2,    SW8_CS2,    SW9_CS2},    
    {1, SW7_CS3,    SW8_CS3,    SW9_CS3},    
    {1, SW7_CS5,    SW8_CS5,    SW9_CS5},    
    {1, SW7_CS4,    SW8_CS4,    SW9_CS4},    
    {1, SW7_CS6,    SW8_CS6,    SW9_CS6},    
    {1, SW7_CS9,    SW8_CS9,    SW9_CS9},    
    {0, SW7_CS1,    SW8_CS1,    SW9_CS1},    
    {0, SW7_CS2,    SW8_CS2,    SW9_CS2},    
    {0, SW7_CS3,    SW8_CS3,    SW9_CS3},    
    {0, SW7_CS4,    SW8_CS4,    SW9_CS4},    
    {0, SW7_CS5,    SW8_CS5,    SW9_CS5},    
    {0, SW7_CS6,    SW8_CS6,    SW9_CS6},    
    {0, SW7_CS7,    SW8_CS7,    SW9_CS7},    
    {0, SW7_CS8,    SW8_CS8,    SW9_CS8},    
    {1, SW10_CS1,    SW11_CS1,    SW12_CS1},    
    {1, SW10_CS2,    SW11_CS2,    SW12_CS2},    
    {1, SW10_CS3,    SW11_CS3,    SW12_CS3},    
    {1, SW10_CS4,    SW11_CS4,    SW12_CS4},    
    {1, SW10_CS5,    SW11_CS5,    SW12_CS5},    
    {1, SW1_CS12,   SW2_CS12,   SW3_CS12},   
    {0, SW10_CS1,    SW11_CS1,    SW12_CS1},    
    {0, SW10_CS2,    SW11_CS2,    SW12_CS2},    
    {0, SW10_CS3,    SW11_CS3,    SW12_CS3},    
    {0, SW10_CS4,    SW11_CS4,    SW12_CS4},    
    {0, SW10_CS5,    SW11_CS5,    SW12_CS5},    
    {0, SW10_CS6,    SW11_CS6,    SW12_CS6},    
    {0, SW10_CS7,    SW11_CS7,    SW12_CS7},    
    {0, SW10_CS8,    SW11_CS8,    SW12_CS8},    
    {0, SW10_CS9,    SW11_CS9,    SW12_CS9},    
    {1, SW7_CS11,   SW8_CS11,   SW9_CS11},   
    {1, SW7_CS10,   SW8_CS10,   SW9_CS10},   
    {1, SW10_CS10,   SW11_CS10,   SW12_CS10},   
    {1, SW10_CS6,    SW11_CS6,    SW12_CS6},    
    {0, SW7_CS12,   SW8_CS12,   SW9_CS12},   
    {0, SW7_CS11,   SW8_CS11,   SW9_CS11},   
    {0, SW7_CS10,   SW8_CS10,   SW9_CS10},   
    {0, SW7_CS9,    SW8_CS9,    SW9_CS9},    
    {0, SW10_CS12,   SW11_CS12,   SW12_CS12},   
    {0, SW10_CS11,   SW11_CS11,   SW12_CS11},   
    {1, SW10_CS11,   SW11_CS11,   SW12_CS11},   
    {1, SW10_CS12,   SW11_CS12,   SW12_CS12},   
    {1, SW10_CS13,   SW11_CS13,   SW12_CS13},   
    
    {1, SW1_CS5,    SW2_CS5,    SW3_CS5},      
    {1, SW1_CS4,    SW2_CS4,    SW3_CS4},       
    {1, SW1_CS3,    SW2_CS3,    SW3_CS3},       
    {1, SW1_CS2,    SW2_CS2,    SW3_CS2},       
    {1, SW1_CS1,    SW2_CS1,    SW3_CS1},         
};