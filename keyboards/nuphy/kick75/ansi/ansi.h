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

#pragma once

#include "quantum.h"

enum custom_keycodes {
    LNK_USB = QK_KB_0,
    LNK_RF,
    LNK_BLE1,
    LNK_BLE2,
    LNK_BLE3,

    MAC_TASK,
    MAC_SEARCH,
    MAC_VOICE,
    MAC_DND,

    WIN_LOCK,
    DEV_RESET,
    SLEEP_MODE,
    BAT_SHOW,
    RGB_TEST,

    SIDE_VAI,
    SIDE_VAD,
    SIDE_MOD,
    SIDE_HUI,
    SIDE_SPI,
    SIDE_SPD,

    MOUSE_JIGGLE,
};


typedef enum {
    RX_IDLE,
    RX_RECEIVING,
    RX_DONE,
    RX_FAIL,
    RX_SUM_ERR,
} uart_rx_state_t;

#define TX_OK      0xE0
#define TX_TIMEOUT 0xE3

#define FUNC_VALID_LEN   32

#define RF_IDLE          0
#define RF_PAIRING       1
#define RF_LINKING       2
#define RF_CONNECT       3
#define RF_DISCONNECT    4
#define RF_SLEEP         5
#define RF_INVALID       0XFE
#define UART_HEAD        0x5A

#define CMD_SLEEP        0XF1
#define CMD_HAND         0XF2
#define CMD_24G_SUSPEND  0XF4

#define CMD_RPT_MS       0XE0  
#define CMD_RPT_BYTE_KB  0XE1 
#define CMD_RPT_BIT_KB   0XE2 
#define CMD_RPT_CONSUME  0XE3  
#define CMD_RPT_SYS      0XE4  

#define CMD_SET_LINK     0XC0
#define CMD_SET_CONFIG   0XC1
#define CMD_SET_NAME     0XC3
#define CMD_CLR_DEVICE   0XC5
#define CMD_NEW_ADV      0XC7
#define CMD_RF_STS_SYSC  0XC9
#define CMD_SET_24G_NAME 0XCA

#define CMD_READ_DATA    0X81

#define LINK_RF_24       0  
#define LINK_BT_1        1 
#define LINK_BT_2        2 
#define LINK_BT_3        3 
#define LINK_USB         4  

#define UART_MAX_LEN     64
typedef struct
{
    uint8_t RXDState;
    uint8_t RXDLen;
    uint8_t RXDOverTime;
    uint8_t TXDLenBack;
    uint8_t TXDOffset;
    uint8_t TXDBuf[UART_MAX_LEN];
    uint8_t RXDBuf[UART_MAX_LEN];
} usart_mgr_t;

typedef struct
{
    uint8_t link_mode;
    uint8_t rf_channel;
    uint8_t ble_channel;
    uint8_t rf_state;
    uint8_t rf_charge;
    uint8_t rf_led;
    uint8_t rf_battery;
    uint8_t sys_sw_state;
} dev_info_t;


#define SYS_SW_WIN        0xa1
#define SYS_SW_MAC        0xa2

#define RF_LINK_SHOW_TIME 300

#define HOST_USB_TYPE     0
#define HOST_RF_TYPE      2

#define LINK_TIMEOUT     (uint16_t)(100 * 120) 
#define SLEEP_TIME_DELAY (uint16_t)(100 * 360)  
#define POWER_DOWN_DELAY (uint16_t)(24)      

typedef struct
{
    uint8_t default_brightness_flag;
    uint8_t ee_side_mode;
    uint8_t ee_side_light;
    uint8_t ee_side_speed;
    uint8_t ee_side_rgb;
    uint8_t ee_side_colour;
    uint8_t sleep_enable;
    uint8_t retain2;
} user_config_t;

void user_config_schedule_save(void);
void side_flash_trigger(uint8_t r, uint8_t g, uint8_t b);