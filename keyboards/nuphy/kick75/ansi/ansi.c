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
#include "hal_usb.h"
#include "usb_main.h"


#define RF_LONG_PRESS_DELAY   30
#define DEV_RESET_PRESS_DELAY 30
#define RGB_TEST_PRESS_DELAY  30

user_config_t user_config; 
dev_info_t dev_info =
{
    .rf_battery = 100,
    .link_mode  = LINK_USB,
    .rf_state   = RF_IDLE,
};

bool f_uart_ack         = 0;
bool f_bat_hold         = 0;
bool f_sys_show         = 0;
bool f_sleep_show       = 0;
bool f_rf_read_data_ok  = 0;
bool f_rf_sts_sysc_ok   = 0;
bool f_rf_new_adv_ok    = 0;
bool f_rf_reset         = 0;
bool f_send_channel     = 0;
bool f_rf_hand_ok       = 0;
bool f_dial_sw_init_ok  = 0;
bool f_goto_sleep       = 0;
bool f_wakeup_prepare   = 0;
bool f_rf_sw_press      = 0;
bool f_dev_reset_press  = 0;
bool f_rgb_test_press   = 0;

uint8_t host_mode;
host_driver_t *m_host_driver   = 0;
uint16_t rf_linking_time       = 0;   
uint16_t rf_link_show_time     = 0; 
uint8_t rf_blink_cnt           = 0;       
uint16_t no_act_time           = 0;       
uint16_t dev_reset_press_delay = 0;  
uint16_t rf_sw_press_delay     = 0;  
uint16_t rgb_test_press_delay  = 0;  
uint8_t rf_sw_temp             = 0;

void rf_uart_init(void);
void rf_device_init(void);
void m_side_led_show(void);
void dev_sts_sync(void);
void uart_send_report_func(void);
void uart_receive_pro(void);
void Sleep_Handle(void);
uint8_t uart_send_cmd(uint8_t cmd, uint8_t ack_cnt, uint8_t delayms);
void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);

void device_reset_show(void);
void device_reset_init(void);
void rgb_test_show(void);

extern uint8_t side_mode;    
extern uint8_t side_light;  
extern uint8_t side_speed;  
extern uint8_t side_rgb;    
extern uint8_t side_colour;  
extern report_keyboard_t *keyboard_report;
extern report_nkro_t *nkro_report;
extern uint8_t uart_bit_report_buf[32];
extern uint8_t bitkb_report_buf[32];
extern uint8_t bytekb_report_buf[8];
extern host_driver_t rf_host_driver;

extern void light_speed_control(uint8_t fast);
extern void light_level_control(uint8_t brighten);
extern void side_colour_control(uint8_t dir);
extern void side_mode_control(uint8_t dir);

void m_gpio_init(void)
{
    gpio_set_pin_output(DC_BOOST_PIN); gpio_write_pin_high(DC_BOOST_PIN);

    gpio_set_pin_output(RGB_DRIVER_SDB1); gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_set_pin_output(RGB_DRIVER_SDB2); gpio_write_pin_high(RGB_DRIVER_SDB2);

    gpio_set_pin_output(NRF_WAKEUP_PIN);
    gpio_write_pin_high(NRF_WAKEUP_PIN);

    gpio_set_pin_input_high(NRF_BOOT_PIN);

    gpio_set_pin_output(NRF_RESET_PIN); gpio_write_pin_low(NRF_RESET_PIN);
    wait_ms(50);
    gpio_write_pin_high(NRF_RESET_PIN);

    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);
}

// 100ms tick: counts hold durations for RF pairing, device reset, RGB test.
void long_press_key(void)
{
    static uint32_t long_press_timer = 0;

    if (timer_elapsed32(long_press_timer) < 100) return;
    long_press_timer = timer_read32();

    if (f_rf_sw_press) {
        rf_sw_press_delay++;
        if (rf_sw_press_delay >= RF_LONG_PRESS_DELAY)
        {
            f_rf_sw_press = 0;
            dev_info.link_mode   = rf_sw_temp;
            dev_info.rf_channel  = rf_sw_temp;
            dev_info.ble_channel = rf_sw_temp;

            uint8_t timeout = 5;
            while (timeout--) {
                uart_send_cmd(CMD_NEW_ADV, 0, 1);
                wait_ms(20);
                uart_receive_pro();                   
                if (f_rf_new_adv_ok) break;
            }
        }
    } else {
        rf_sw_press_delay = 0;
    }

    if (f_dev_reset_press) {
        dev_reset_press_delay++;
        if (dev_reset_press_delay >= DEV_RESET_PRESS_DELAY)  {
            f_dev_reset_press = 0;

            if (dev_info.link_mode != LINK_USB) {
                if (dev_info.link_mode != LINK_RF_24) {
                    dev_info.link_mode   = LINK_BT_1;
                    dev_info.ble_channel = LINK_BT_1;
                    dev_info.rf_channel  = LINK_BT_1;
                }
            } else {
                dev_info.ble_channel = LINK_BT_1;
                dev_info.rf_channel  = LINK_BT_1;
            }

            uart_send_cmd(CMD_SET_LINK, 10, 10);
            wait_ms(500);
            uart_send_cmd(CMD_CLR_DEVICE, 10, 10);

            eeconfig_init();
            device_reset_show();
            device_reset_init();

            if (dev_info.sys_sw_state == SYS_SW_MAC) {
                default_layer_set(1 << 0);
            } else {
                default_layer_set(1 << 2);
            }
        }
    } else {
        dev_reset_press_delay = 0;
    }

    if (f_rgb_test_press) {
        rgb_test_press_delay++;
        if (rgb_test_press_delay >= RGB_TEST_PRESS_DELAY) {
            f_rgb_test_press = 0;
            rgb_test_show(); 
        }
    } else {
        rgb_test_press_delay = 0;
    }
}

void m_break_all_key(void)
{
    uint8_t report_buf[16];
    bool nkro_temp = keymap_config.nkro; 

    clear_weak_mods();
    clear_mods();
    clear_keyboard();

    keymap_config.nkro = 1;
    memset(nkro_report, 0, sizeof(report_nkro_t));
    host_nkro_send(nkro_report);
    wait_ms(10);

    keymap_config.nkro = 0;
    memset(keyboard_report, 0, sizeof(report_keyboard_t));
    host_keyboard_send(keyboard_report);
    wait_ms(10);

    keymap_config.nkro = nkro_temp;

    if (dev_info.link_mode != LINK_USB) {
        memset(report_buf, 0, 16);
        uart_send_report(CMD_RPT_BIT_KB, report_buf, 16);
        wait_ms(10);
        uart_send_report(CMD_RPT_BYTE_KB, report_buf, 8);
        wait_ms(10);
    }

    memset(uart_bit_report_buf, 0, sizeof(uart_bit_report_buf));
    memset(bitkb_report_buf, 0, sizeof(bitkb_report_buf));
    memset(bytekb_report_buf, 0, sizeof(bytekb_report_buf));
}

static void switch_dev_link(uint8_t mode)
{
    if (mode > LINK_USB) return;
    m_break_all_key();    

    dev_info.link_mode = mode; 
    dev_info.rf_state = RF_IDLE;
    f_send_channel = 1;

    if (mode == LINK_USB) {
        host_mode = HOST_USB_TYPE;    
        host_set_driver(m_host_driver); 
        rf_link_show_time = 0;        
    }
    else {
        host_mode = HOST_RF_TYPE; 
        host_set_driver(&rf_host_driver);           

    }
}

// Polls USB/BT and Win/Mac hardware switches with debounce.
void dial_sw_scan(void)
{
    uint8_t dial_scan               = 0;
    static uint8_t dial_save        = 0xf0;
    static uint8_t debounce         = 0;
    static uint32_t dial_scan_timer = 0;
    static bool flag_power_on       = 1;

    if (!flag_power_on) {
        if (timer_elapsed32(dial_scan_timer) < 20) return;
    }
    dial_scan_timer = timer_read32();

    gpio_set_pin_input_high(DEV_MODE_PIN);
    gpio_set_pin_input_high(SYS_MODE_PIN);

    if (gpio_read_pin(DEV_MODE_PIN)) dial_scan |= 0X01;
    if (gpio_read_pin(SYS_MODE_PIN)) dial_scan |= 0X02;

    if (dial_save != dial_scan) {
        m_break_all_key(); 

        no_act_time     = 0;  
        rf_linking_time = 0; 

        dial_save         = dial_scan;
        debounce          = 25;  
        f_dial_sw_init_ok = 0;  
        return;
    } else if (debounce) {
        debounce--;
        return;
    }

    if (dial_scan & 0x01) {
        if (dev_info.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (dev_info.link_mode != dev_info.rf_channel) {
            switch_dev_link(dev_info.rf_channel);
        }
    }

    if (dial_scan & 0x02) {
        if (dev_info.sys_sw_state != SYS_SW_MAC) {
            f_sys_show = 1;
            default_layer_set(1 << 0);  
            dev_info.sys_sw_state = SYS_SW_MAC;
            keymap_config.nkro    = 0; 
            m_break_all_key();        
        }
    } else {
        if (dev_info.sys_sw_state != SYS_SW_WIN) {
            f_sys_show = 1;
            default_layer_set(1 << 2);  
            dev_info.sys_sw_state = SYS_SW_WIN;
            keymap_config.nkro    = 1;  
            m_break_all_key();        
        }
    }

    if (f_dial_sw_init_ok == 0) {
        f_dial_sw_init_ok = 1; 
        flag_power_on     = false;

        if (dev_info.link_mode != LINK_USB) {
            host_set_driver(&rf_host_driver);
        }
    }
}

// Boot-time switch sampling (~10ms debounce).
void m_power_on_dial_sw_scan(void)
{
    uint8_t dial_scan_dev = 0;
    uint8_t dial_scan_sys = 0;
    uint8_t dial_check_dev = 0;
    uint8_t dial_check_sys = 0;
    uint8_t debounce = 0;

    gpio_set_pin_input_high(DEV_MODE_PIN);      
    gpio_set_pin_input_high(SYS_MODE_PIN);     

    for(debounce=0; debounce<10; debounce++) {
        dial_scan_dev = 0;
        dial_scan_sys = 0;
        if (gpio_read_pin(DEV_MODE_PIN)) dial_scan_dev = 0x01;
        else dial_scan_dev = 0;
        if (gpio_read_pin(SYS_MODE_PIN)) dial_scan_sys = 0x01;
        else dial_scan_sys = 0;
        if((dial_scan_dev != dial_check_dev)||(dial_scan_sys != dial_check_sys))
        {
            dial_check_dev = dial_scan_dev;
            dial_check_sys = dial_scan_sys;
            debounce = 0;
        }
        wait_ms(1);
    }
    // RF link mode
    if (dial_scan_dev) {
        if (dev_info.link_mode != LINK_USB) {
            switch_dev_link(LINK_USB);
        }
    } else {
        if (dev_info.link_mode != dev_info.rf_channel) {
            switch_dev_link(dev_info.rf_channel);
        }
    }

    if (dial_scan_sys) {
        if (dev_info.sys_sw_state != SYS_SW_MAC) {
            default_layer_set(1 << 0);  
            dev_info.sys_sw_state = SYS_SW_MAC;
            keymap_config.nkro    = 0; 
            m_break_all_key();  
        }
    } else {
        if (dev_info.sys_sw_state != SYS_SW_WIN) {
            default_layer_set(1 << 2);  
            dev_info.sys_sw_state = SYS_SW_WIN;
            keymap_config.nkro    = 1; 
            m_break_all_key();     
        }
    }
}

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if(!process_record_user(keycode, record)){
        return false;
    }
    no_act_time = 0;
    switch (keycode) {
        case LNK_USB:
            if (record->event.pressed) {
                m_break_all_key();
            } else {
                dev_info.link_mode = LINK_USB;
                uart_send_cmd(CMD_SET_LINK, 10, 10);
                rf_blink_cnt = 3;
            }
            return false;

        case LNK_RF:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_RF_24;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE1:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_1;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE2:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_2;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case LNK_BLE3:
            if (record->event.pressed) {
                if (dev_info.link_mode != LINK_USB) {
                    rf_sw_temp    = LINK_BT_3;
                    f_rf_sw_press = 1;
                    m_break_all_key();
                }
            } else if (f_rf_sw_press) {
                f_rf_sw_press = 0;
                if (rf_sw_press_delay < RF_LONG_PRESS_DELAY) {
                    dev_info.link_mode   = rf_sw_temp;
                    dev_info.rf_channel  = rf_sw_temp;
                    dev_info.ble_channel = rf_sw_temp;
                    uart_send_cmd(CMD_SET_LINK, 10, 20);
                }
            }
            return false;

        case MAC_TASK:
            if (record->event.pressed) {
                host_consumer_send(0x029F);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_SEARCH:
            if (record->event.pressed) {
                register_code(KC_LGUI);
                register_code(KC_SPACE);
                uart_send_report_func();
                wait_ms(50);
                unregister_code(KC_LGUI);
                unregister_code(KC_SPACE);
            }
            return false;

        case MAC_VOICE:
            if (record->event.pressed) {
                host_consumer_send(0xcf);
            } else {
                host_consumer_send(0);
            }
            return false;

        case MAC_DND:
            if (record->event.pressed) {
                host_system_send(0x9b);
            } else {
                host_system_send(0);
            }
            return false;

        case SIDE_VAI:
            if (record->event.pressed) {
                light_level_control(1);
            }
            return false;

        case SIDE_VAD:
            if (record->event.pressed) {
                light_level_control(0);
            }
            return false;

        case SIDE_MOD:
            if (record->event.pressed) {
                side_mode_control(1);
            }
            return false;

        case SIDE_HUI:
            if (record->event.pressed) {
                side_colour_control(1);
            }
            return false;

        case SIDE_SPI:
            if (record->event.pressed) {
                light_speed_control(1);
            }
            return false;

        case SIDE_SPD:
            if (record->event.pressed) {
                light_speed_control(0);
            }
            return false;

        case DEV_RESET:
            if (record->event.pressed) {
                f_dev_reset_press = 1;
                m_break_all_key(); 
            } else {
                f_dev_reset_press = 0;
            }
            return false;

        case SLEEP_MODE:
            if (record->event.pressed) {
                if(user_config.sleep_enable) user_config.sleep_enable = false;
                else user_config.sleep_enable = true;
                f_sleep_show       = 1;
                user_config_schedule_save();
            }
            return false;

        case BAT_SHOW:
            if (record->event.pressed) {
                f_bat_hold = !f_bat_hold;
            }
            return false;

        case WIN_LOCK:
            if (record->event.pressed) {
                keymap_config.no_gui = !keymap_config.no_gui;
                eeconfig_update_keymap(&keymap_config);
                side_flash_trigger(0, 128, 0);  // green
            }
            return false;

        case RGB_TEST:
            if (record->event.pressed) {
                f_rgb_test_press = 1;
            } else {
                f_rgb_test_press = 0;
            }
            return false;

        case MOUSE_JIGGLE:
            if (record->event.pressed) {
                extern bool    jiggler_active;
                extern uint8_t jiggler_step;
                jiggler_active = !jiggler_active;
                jiggler_step   = 0;
                side_flash_trigger(128, 0, 64);  // pink (matches key indicator)
            }
            return false;

        default:
            return true;
    }
}


// 10ms tick counters for RF link, inactivity, and blink timers.
void timer_pro(void)
{
    static uint32_t interval_timer = 0;
    static bool f_first            = true;

    if (f_first) {
        f_first        = false;
        interval_timer = timer_read32();
        m_host_driver  = host_get_driver();
    }

    if (timer_elapsed32(interval_timer) < 10) {
        return;
    } else if (timer_elapsed32(interval_timer) > 20) {
        interval_timer = timer_read32();
    } else {
        interval_timer += 10; 
    }

    if (rf_link_show_time < RF_LINK_SHOW_TIME)
        rf_link_show_time++;

    if (no_act_time < 0xffff)
        no_act_time++;

    if (rf_linking_time < 0xffff)
        rf_linking_time++;
}


// Load user_config from EEPROM; seed defaults on first boot.
void m_londing_eeprom_data(void)
{
    eeconfig_read_user_datablock(&user_config, 0, sizeof(user_config));
    if (user_config.default_brightness_flag != 0xA5) {
        rgb_matrix_sethsv(255, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS - RGB_MATRIX_VAL_STEP * 2); 
        user_config.default_brightness_flag = 0xA5;
        user_config.ee_side_mode            = side_mode;
        user_config.ee_side_light           = side_light;
        user_config.ee_side_speed           = side_speed;
        user_config.ee_side_rgb             = side_rgb;
        user_config.ee_side_colour          = side_colour;
        user_config.sleep_enable            = true;
        eeconfig_update_user_datablock(&user_config, 0, sizeof(user_config));  
    } else {
        side_mode   = user_config.ee_side_mode;
        side_light  = user_config.ee_side_light;
        side_speed  = user_config.ee_side_speed;
        side_rgb    = user_config.ee_side_rgb;
        side_colour = user_config.ee_side_colour;
    }
}


void keyboard_post_init_kb(void)
{
    m_gpio_init(); 
    rf_uart_init();               
    wait_ms(500);             
    rf_device_init();           

    m_break_all_key();           
    m_londing_eeprom_data();    
    m_power_on_dial_sw_scan();  
    keyboard_post_init_user();
}

// Per-key indicator overlays (full brightness, bypasses RGB scaler).
bool rgb_matrix_indicators_kb(void)
{
    if (!rgb_matrix_indicators_user()) {
        return false;
    }

    extern bool jiggler_active;
    extern bool boot_hold_active;

    // Win/Cmd lock → green on GUI key.
    if (keymap_config.no_gui) {
        uint8_t col = (dev_info.sys_sw_state == SYS_SW_MAC) ? 2 : 1;
        uint8_t led = g_led_config.matrix_co[5][col];
        if (led != NO_LED) rgb_matrix_set_color(led, 0, 255, 0);
    }

    // Caps Lock → cyan on KC_CAPS (matches side-strip colour).
    bool caps_on = (dev_info.link_mode == LINK_USB)
        ? host_keyboard_led_state().caps_lock
        : (dev_info.rf_led & 0x02);
    if (caps_on) {
        uint8_t led = g_led_config.matrix_co[3][0];
        if (led != NO_LED) rgb_matrix_set_color(led, 0, 255, 255);
    }

    // Mouse jiggler → pink on LCTL.
    if (jiggler_active) {
        uint8_t led = g_led_config.matrix_co[5][0];
        if (led != NO_LED) rgb_matrix_set_color(led, 255, 0, 128);
    }

    // BOOT_HOLD → blink Esc red at ~3 Hz while held.
    if (boot_hold_active) {
        uint8_t led = g_led_config.matrix_co[0][0];
        if (led != NO_LED) {
            bool on = (timer_read() / 166) % 2;
            rgb_matrix_set_color(led, on ? 255 : 0, 0, 0);
        }
    }

    return true;
}

// Deferred EEPROM save — coalesces rapid config changes into one write.
#define USER_CONFIG_SAVE_DELAY_MS 500

static bool     user_config_dirty      = false;
static uint16_t user_config_dirty_time = 0;

void user_config_schedule_save(void) {
    user_config_dirty      = true;
    user_config_dirty_time = timer_read();
}

static void user_config_maybe_save(void) {
    if (user_config_dirty && timer_elapsed(user_config_dirty_time) > USER_CONFIG_SAVE_DELAY_MS) {
        eeconfig_update_user_datablock(&user_config, 0, sizeof(user_config));
        user_config_dirty = false;
    }
}

// Mouse jiggler — cross pattern (up/right/down/left), each followed by
// a return step so the cursor always drifts back to the origin over one
// full 8-step cycle. Intentionally noticeable so you can't forget it's on.
#define JIGGLER_INTERVAL_MS 2000
#define JIGGLER_NUDGE_PX    80

bool            jiggler_active    = false;
uint8_t         jiggler_step      = 0;
static uint32_t jiggler_last_time = 0;

static void jiggler_task(void) {
    if (!jiggler_active) return;
    if (timer_elapsed32(jiggler_last_time) < JIGGLER_INTERVAL_MS) return;
    jiggler_last_time = timer_read32();

    // Even steps = outward move, odd steps = matching return.
    report_mouse_t r = {0};
    switch (jiggler_step) {
        case 0: r.y = -JIGGLER_NUDGE_PX; break;  // up
        case 1: r.y =  JIGGLER_NUDGE_PX; break;  // return
        case 2: r.x =  JIGGLER_NUDGE_PX; break;  // right
        case 3: r.x = -JIGGLER_NUDGE_PX; break;  // return
        case 4: r.y =  JIGGLER_NUDGE_PX; break;  // down
        case 5: r.y = -JIGGLER_NUDGE_PX; break;  // return
        case 6: r.x = -JIGGLER_NUDGE_PX; break;  // left
        case 7: r.x =  JIGGLER_NUDGE_PX; break;  // return
    }
    host_mouse_send(&r);
    jiggler_step = (jiggler_step + 1) & 0x07;
}

// Auto-sleep / wake state machine (USB suspend, RF inactivity).
void Sleep_Handle(void) {
    static uint32_t delay_step_timer = 0;
    static uint8_t  usb_suspend_debounce;
    static uint32_t rf_disconnect_time = 0;

    /* 50ms interval */
    if (timer_elapsed32(delay_step_timer) < 50) return;
    delay_step_timer = timer_read32();

    if (f_goto_sleep) {
        f_goto_sleep = 0;

        if(user_config.sleep_enable) {
            if (dev_info.rf_state == RF_CONNECT)
                uart_send_cmd(CMD_SET_CONFIG, 5, 5);
            else
                uart_send_cmd(CMD_SLEEP, 5, 5);

            // power off led
            gpio_write_pin_low(DC_BOOST_PIN);
            gpio_write_pin_low(RGB_DRIVER_SDB1);
            gpio_write_pin_low(RGB_DRIVER_SDB2);
        }

        f_wakeup_prepare = 1;
    }

    if (f_wakeup_prepare && (no_act_time < 10)) {
        f_wakeup_prepare = 0;

        gpio_write_pin_high(DC_BOOST_PIN);
        gpio_write_pin_high(RGB_DRIVER_SDB1);
        gpio_write_pin_high(RGB_DRIVER_SDB2);

        uart_send_cmd(CMD_HAND, 0, 1);

        if (dev_info.link_mode == LINK_USB) {
            #define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED (2U)
            if ((USB_DRIVER.status & USB_GETSTATUS_REMOTE_WAKEUP_ENABLED) ) {
                usb_lld_wakeup_host(&USB_DRIVER);
                wait_ms(50);
                uint8_t timeout = 10;
                while ((USB_DRIVER.state == USB_SUSPENDED) && (timeout--)) {
                    usbWakeupHost(&USB_DRIVER);
                    restart_usb_driver(&USB_DRIVER);
                    wait_ms(50);
                }
                m_break_all_key();
            }
        }
    }

    if (f_goto_sleep || f_wakeup_prepare) return;

    if (dev_info.link_mode == LINK_USB) {
        if (USB_DRIVER.state == USB_SUSPENDED) {
            usb_suspend_debounce++;
            if (usb_suspend_debounce >= 20) {
                f_goto_sleep = 1;
            }
        } else {
            usb_suspend_debounce = 0;
        }
    } else if (dev_info.rf_state == RF_CONNECT) {
        rf_disconnect_time = 0;
        if (no_act_time >= SLEEP_TIME_DELAY) {
            f_goto_sleep = 1;
        }
    } else if (rf_linking_time >= LINK_TIMEOUT) {
        rf_linking_time = 0;
        f_goto_sleep    = 1;
    } else if (dev_info.rf_state == RF_DISCONNECT) {
        rf_disconnect_time++;
        if (rf_disconnect_time > 5 * 20) {
            rf_disconnect_time = 0;
            f_goto_sleep = 1;
        }
    }
}

void housekeeping_task_kb(void)
{
    timer_pro();

    uart_receive_pro();

    uart_send_report_func();

    dev_sts_sync();

    long_press_key();

    dial_sw_scan();

    m_side_led_show();

    Sleep_Handle();

    user_config_maybe_save();

    jiggler_task();
}