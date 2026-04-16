// Copyright 2023 Persama (@Persama)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "ansi.h"
#include "uart.h"  // qmk uart.h

usart_mgr_t Usart_Mgr;
#define RX_SBYTE    Usart_Mgr.RXDBuf[0]
#define RX_CMD      Usart_Mgr.RXDBuf[1]
#define RX_ACK      Usart_Mgr.RXDBuf[2]
#define RX_LEN      Usart_Mgr.RXDBuf[3]
#define RX_DAT      Usart_Mgr.RXDBuf[4]

extern bool f_uart_ack;
extern bool f_rf_read_data_ok;
extern bool f_rf_sts_sysc_ok;
extern bool f_rf_new_adv_ok;
extern bool f_rf_reset;
extern bool f_rf_hand_ok;
extern bool f_goto_sleep;

uint8_t  uart_bit_report_buf[32] = {0};
uint8_t  bitkb_report_buf[32]    = {0};
uint8_t  bytekb_report_buf[8]    = {0};
uint8_t  sync_lost               = 0;
uint8_t  disconnect_delay        = 0;
bool     uart_repeat_flag        = 0;

extern dev_info_t dev_info;
extern host_driver_t  *m_host_driver;
extern uint8_t         host_mode;
extern uint8_t         rf_blink_cnt;
extern uint16_t        rf_link_show_time;
extern uint16_t        rf_linking_time;
extern uint16_t        no_act_time;
extern bool            f_send_channel;
extern bool            f_dial_sw_init_ok;

void           uart_init(uint32_t baud); // qmk uart.c
void           uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size);
void           UART_Send_Bytes(uint8_t *Buffer, uint32_t Length);
uint8_t        get_checksum(uint8_t *buf, uint8_t len);
void           uart_receive_pro(void);
void           m_break_all_key(void);

/* Forward declaration of the RF host driver vtable. The definition is
 * at the bottom of this file, but dev_sts_sync() below passes its
 * address to host_set_driver().
 */
extern host_driver_t rf_host_driver;

// Diffs NKRO bit reports → emits byte or bit report to RF UART.
bool f_bit_kb_act = 0;
static void uart_auto_nkey_send(uint8_t *pre_bit_report, uint8_t *now_bit_report, uint8_t size)
{
    uint8_t i, j, byte_index;
    uint8_t change_mask, offset_mask;
    uint8_t key_code = 0;
    bool f_byte_send = 0, f_bit_send = 0;

    if (pre_bit_report[0] ^ now_bit_report[0]) {
        bytekb_report_buf[0] = now_bit_report[0];
        f_byte_send          = 1;
    }

    for (i = 1; i < size; i++) {
        change_mask = pre_bit_report[i] ^ now_bit_report[i];
        offset_mask = 1;
        for (j = 0; j < 8; j++) {
            if (change_mask & offset_mask) {
                if (now_bit_report[i] & offset_mask) {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == 0) {
                            bytekb_report_buf[byte_index] = key_code;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] |= offset_mask;
                        f_bit_send = 1;
                    }
                } else {
                    for (byte_index = 2; byte_index < 8; byte_index++) {
                        if (bytekb_report_buf[byte_index] == key_code) {
                            bytekb_report_buf[byte_index] = 0;
                            f_byte_send                   = 1;
                            break;
                        }
                    }
                    if (byte_index >= 8) {
                        uart_bit_report_buf[i] &= ~offset_mask;
                        f_bit_send = 1;
                    }
                }
            }
            key_code++;
            offset_mask <<= 1;
        }
    }

    if (f_bit_send) {
        f_bit_kb_act = 1;
        uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
    }

    if (f_byte_send) {
        uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
    }
}


// RF report send loop — diffs current vs last-sent, emits on change.
void uart_send_report_func(void)
{
    static uint32_t interval_timer = 0;

    if (dev_info.link_mode == LINK_USB) return;

    if (timer_elapsed32(interval_timer) > 50) {
        interval_timer = timer_read32();
        if (no_act_time <= 2000) {
            uart_send_report(CMD_RPT_BYTE_KB, bytekb_report_buf, 8);
            wait_us(200);

            if(f_bit_kb_act)
            uart_send_report(CMD_RPT_BIT_KB, uart_bit_report_buf, 16);
        }
        else {
            f_bit_kb_act = 0;
        }
    }
}

/**
 * @brief  Uart send consumer keys report.
 * @note Call in rf_driver.c
 */
void uart_send_consumer_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_CONSUME, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send mouse keys report.
 * @note Call in rf_driver.c
 */
void uart_send_mouse_report(report_mouse_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_MS, &report->buttons, 5);
}

/**
 * @brief  Uart send system keys report.
 * @note Call in rf_driver.c
 */
void uart_send_system_report(report_extra_t *report) {
    no_act_time = 0;
    uart_send_report(CMD_RPT_SYS, (uint8_t *)(&report->usage), 2);
}

/**
 * @brief  Uart send byte keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_keyboard(report_keyboard_t *report) {
    no_act_time      = 0;
    report->reserved = 0;
    uart_send_report(CMD_RPT_BYTE_KB, &report->mods, 8);
    memcpy(bytekb_report_buf, &report->mods, 8);
}

/**
 * @brief  Uart send bit keys report.
 * @note Call in rf_driver.c
 */
void uart_send_report_nkro(report_nkro_t *report) {
    no_act_time = 0;
    uart_auto_nkey_send(bitkb_report_buf, &nkro_report->mods, NKRO_REPORT_BITS + 1);
    memcpy(&bitkb_report_buf[0], &nkro_report->mods, NKRO_REPORT_BITS + 1);
}

/**
 * @brief  Parsing the data received from the RF module.
 */
void RF_Protocol_Receive(void) {
    uint8_t i, check_sum = 0;

    if (Usart_Mgr.RXDState == RX_DONE) {
        f_uart_ack = 1;
        sync_lost = 0;

        if (Usart_Mgr.RXDLen > 4) {
            if((Usart_Mgr.RXDLen - 5) != RX_LEN) 
                return;

            for (i = 0; i < RX_LEN; i++)
                check_sum += Usart_Mgr.RXDBuf[4 + i];

            if (check_sum != Usart_Mgr.RXDBuf[4 + i]) {
                Usart_Mgr.RXDState = RX_SUM_ERR;
                return;
            }
        } else if (Usart_Mgr.RXDLen == 3) {
            if (Usart_Mgr.RXDBuf[2] == 0xA0) {
                f_uart_ack = 1;
            }
            else {
                return;
            }
        } else {
            return;
        }

        switch (RX_CMD) {
            case CMD_HAND: {
                f_rf_hand_ok = 1;
                break;
            }

            case CMD_24G_SUSPEND: {
                f_goto_sleep = 1;
                break;
            }

            case CMD_NEW_ADV: {
                f_rf_new_adv_ok = 1;
                break;
            }

            case CMD_RF_STS_SYSC: {
                static uint8_t error_cnt = 0;

                if (dev_info.link_mode == Usart_Mgr.RXDBuf[4]) {
                    error_cnt = 0;

                    dev_info.rf_state = Usart_Mgr.RXDBuf[5];

                    if ((dev_info.rf_state == RF_CONNECT) && ((Usart_Mgr.RXDBuf[6] & 0xf8) == 0)) {
                        dev_info.rf_led = Usart_Mgr.RXDBuf[6];
                    }

                    dev_info.rf_charge = Usart_Mgr.RXDBuf[7];

                    if (Usart_Mgr.RXDBuf[8] <= 100) dev_info.rf_battery = Usart_Mgr.RXDBuf[8];
                    if (dev_info.rf_charge & 0x01) dev_info.rf_battery = 100;
                }
                else {
                    if (dev_info.rf_state != RF_INVALID) {
                        if (error_cnt >= 5) {
                            error_cnt      = 0;
                            f_send_channel = 1;
                        } else {
                            error_cnt++;
                        }
                    }
                }

                f_rf_sts_sysc_ok = 1;
                break;
            }

            case CMD_READ_DATA: {
                // Payload starts at RXDBuf[4]; we only need offsets 4, 5, 6
                // from that payload (= RXDBuf[8..10]) for the link config.
                uint8_t link_mode_byte   = Usart_Mgr.RXDBuf[8];
                uint8_t rf_channel_byte  = Usart_Mgr.RXDBuf[9];
                uint8_t ble_channel_byte = Usart_Mgr.RXDBuf[10];

                if (link_mode_byte <= LINK_USB) {
                    dev_info.link_mode = link_mode_byte;
                }

                if (rf_channel_byte < LINK_USB) {
                    dev_info.rf_channel = rf_channel_byte;
                }

                if ((ble_channel_byte <= LINK_BT_3) && (ble_channel_byte >= LINK_BT_1)) {
                    dev_info.ble_channel = ble_channel_byte;
                }

                f_rf_read_data_ok = 1;
                break;
            }
        }

        Usart_Mgr.RXDLen      = 0;
        Usart_Mgr.RXDState    = RX_IDLE;
        Usart_Mgr.RXDOverTime = 0;
    }
}

// Send command frame to RF module (wait_ack=0 → fire-and-forget).
uint8_t uart_send_cmd(uint8_t cmd, uint8_t wait_ack, uint8_t delayms) {
    wait_ms(delayms);

    memset(&Usart_Mgr.TXDBuf[0], 0, UART_MAX_LEN);

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = cmd;
    Usart_Mgr.TXDBuf[2] = 0x00;

    switch (cmd) {
        case CMD_SLEEP: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_HAND: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_RF_STS_SYSC: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;
            break;
        }

        case CMD_SET_LINK: {
            dev_info.rf_state   = RF_LINKING;
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = dev_info.link_mode;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            break;
        }

        case CMD_NEW_ADV: {
            dev_info.rf_state   = RF_PAIRING;
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = dev_info.link_mode;
            Usart_Mgr.TXDBuf[5] = 1;
            Usart_Mgr.TXDBuf[6] = dev_info.link_mode + 1;

            rf_linking_time  = 0;
            disconnect_delay = 0xff;
            f_rf_new_adv_ok  = 0;
            break;
        }

        case CMD_CLR_DEVICE: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = 0;
            Usart_Mgr.TXDBuf[5] = 0;
            break;
        }

        case CMD_SET_CONFIG: {
            Usart_Mgr.TXDBuf[3] = 1;
            Usart_Mgr.TXDBuf[4] = POWER_DOWN_DELAY;
            Usart_Mgr.TXDBuf[5] = POWER_DOWN_DELAY;
            break;
        }

        case CMD_SET_NAME: {
            Usart_Mgr.TXDBuf[3]  = 15;                                                       // data len
            Usart_Mgr.TXDBuf[4]  = 1;                                                        // name suffix type
            Usart_Mgr.TXDBuf[5]  = 13;                                                       // data: ble name len
            Usart_Mgr.TXDBuf[6]  = 'N';                                                      // data: ble name
            Usart_Mgr.TXDBuf[7]  = 'u';                                                      // data: ble name
            Usart_Mgr.TXDBuf[8]  = 'P';                                                      // data: ble name
            Usart_Mgr.TXDBuf[9]  = 'h';                                                      // data: ble name
            Usart_Mgr.TXDBuf[10] = 'y';                                                      // data: ble name
            Usart_Mgr.TXDBuf[11] = ' ';                                                      // data: ble name
            Usart_Mgr.TXDBuf[12] = 'K';                                                      // data: ble name
            Usart_Mgr.TXDBuf[13] = 'i';                                                      // data: ble name
            Usart_Mgr.TXDBuf[14] = 'c';                                                      // data: ble name
            Usart_Mgr.TXDBuf[15] = 'k';                                                      // data: ble name
            Usart_Mgr.TXDBuf[16] = '7';                                                      // data: ble name
            Usart_Mgr.TXDBuf[17] = '5';                                                      // data: ble name
            Usart_Mgr.TXDBuf[18] = '-';                                                      // data: ble name
            Usart_Mgr.TXDBuf[19] = get_checksum(Usart_Mgr.TXDBuf + 4, Usart_Mgr.TXDBuf[3]);  // sum
            break;
        }

        case CMD_SET_24G_NAME: {
            Usart_Mgr.TXDBuf[3]  = 40;          // uart data len
            Usart_Mgr.TXDBuf[4]  = 40;          // name valid len
            Usart_Mgr.TXDBuf[5]  = 3;           
            Usart_Mgr.TXDBuf[6]  = 'N';
            Usart_Mgr.TXDBuf[8]  = 'u';
            Usart_Mgr.TXDBuf[10] = 'P';
            Usart_Mgr.TXDBuf[12] = 'h';
            Usart_Mgr.TXDBuf[14] = 'y';
            Usart_Mgr.TXDBuf[16] = ' ';
            Usart_Mgr.TXDBuf[18] = 'K';
            Usart_Mgr.TXDBuf[20] = 'i';
            Usart_Mgr.TXDBuf[22] = 'c';
            Usart_Mgr.TXDBuf[24] = 'k';
            Usart_Mgr.TXDBuf[26] = '7';
            Usart_Mgr.TXDBuf[28] = '5';
            Usart_Mgr.TXDBuf[30] = ' ';
            Usart_Mgr.TXDBuf[32] = 'D';
            Usart_Mgr.TXDBuf[34] = 'o';
            Usart_Mgr.TXDBuf[36] = 'n';
            Usart_Mgr.TXDBuf[38] = 'g';
            Usart_Mgr.TXDBuf[40] = 'l';
            Usart_Mgr.TXDBuf[42] = 'e';
            Usart_Mgr.TXDBuf[44] = get_checksum(Usart_Mgr.TXDBuf + 4, Usart_Mgr.TXDBuf[3]);  // sum
            break;
        }

        case CMD_READ_DATA: {
            Usart_Mgr.TXDBuf[3] = 2;
            Usart_Mgr.TXDBuf[4] = 0x00;
            Usart_Mgr.TXDBuf[5] = FUNC_VALID_LEN;
            Usart_Mgr.TXDBuf[6] = FUNC_VALID_LEN;
            break;
        }

        default:
            break;
    }

    f_uart_ack = 0;
    UART_Send_Bytes(Usart_Mgr.TXDBuf, Usart_Mgr.TXDBuf[3] + 5);

    if (wait_ack) {
        while (wait_ack--) {
            wait_ms(1);
            if (f_uart_ack) return TX_OK;
        }
    } else {
        return TX_OK;
    }

    return TX_TIMEOUT;
}

// Polls RF module status (link, battery, LEDs) every 200ms.
void dev_sts_sync(void) {
    static uint32_t interval_timer  = 0;
    static uint8_t  link_state_temp = RF_DISCONNECT;

    if (timer_elapsed32(interval_timer) < 200)
        return;
    else
        interval_timer = timer_read32();

    if (f_rf_reset) {
        f_rf_reset = 0;
        wait_ms(100);
        gpio_write_pin_low(NRF_RESET_PIN);
        wait_ms(50);
        gpio_write_pin_high(NRF_RESET_PIN);
        wait_ms(50);
    }
    else if (f_send_channel) {
        f_send_channel = 0;
        uart_send_cmd(CMD_SET_LINK, 10, 10);
    }

    if (dev_info.link_mode == LINK_USB) {
        if (host_mode != HOST_USB_TYPE) {
            host_mode = HOST_USB_TYPE;
            host_set_driver(m_host_driver);
            m_break_all_key();
        }
        rf_blink_cnt = 0;
    }
    else {
        if (host_mode != HOST_RF_TYPE) {
            host_mode = HOST_RF_TYPE;
            m_break_all_key();
            host_set_driver(&rf_host_driver);
        }

        if (dev_info.rf_state != RF_CONNECT) {
            if (disconnect_delay >= 10) {
                rf_blink_cnt    = 3;
                rf_link_show_time = 0;
                link_state_temp = dev_info.rf_state;
            } else {
                disconnect_delay++;
            }
        }
        else if (dev_info.rf_state == RF_CONNECT) {
            rf_linking_time  = 0;
            disconnect_delay = 0;
            rf_blink_cnt     = 0;

            if (link_state_temp != RF_CONNECT) {
                link_state_temp   = RF_CONNECT;
                rf_link_show_time = 0;
                if (dev_info.link_mode == LINK_RF_24) {
                    uart_send_cmd(CMD_SET_24G_NAME, 10, 30);
                }
            }
        }
    }

    uart_send_cmd(CMD_RF_STS_SYSC, 1, 1);

    if (dev_info.link_mode != LINK_USB) {
        if (++sync_lost >= 5) {
            sync_lost  = 0;
            f_rf_reset = 1;
        }
    }
}

// Transmit raw bytes to RF module (3x retries if uart_repeat_flag set).
void UART_Send_Bytes(uint8_t *Buffer, uint32_t Length) {
    if(uart_repeat_flag) {
        for(uint8_t i = 0;i<3;i++)
        {
            gpio_write_pin_low(NRF_WAKEUP_PIN);
            wait_us(50);
        
            uart_transmit(Buffer, Length);
        
            wait_us(50 + Length * 32);
            gpio_write_pin_high(NRF_WAKEUP_PIN);  
        
            wait_us(200);      
        }        
    } else {
            gpio_write_pin_low(NRF_WAKEUP_PIN);
            wait_us(50);
        
            uart_transmit(Buffer, Length);
        
            wait_us(50 + Length * 32);
            gpio_write_pin_high(NRF_WAKEUP_PIN);          
    }
}

// XOR checksum: sum the bytes in buf, then XOR with UART_HEAD.
uint8_t get_checksum(uint8_t *buf, uint8_t len) {
    uint8_t i;
    uint8_t checksum = 0;

    for (i = 0; i < len; i++)
        checksum += *buf++;

    checksum ^= UART_HEAD;

    return checksum;
}

// Wraps a HID report in the RF frame format (head + type + ack +
// length + payload + checksum) and sends it to the RF module.
void uart_send_report(uint8_t report_type, uint8_t *report_buf, uint8_t report_size) {
    if (f_dial_sw_init_ok == 0) return;
    if (dev_info.link_mode == LINK_USB) return;
    if (dev_info.rf_state != RF_CONNECT) return;

    Usart_Mgr.TXDBuf[0] = UART_HEAD;
    Usart_Mgr.TXDBuf[1] = report_type;
    Usart_Mgr.TXDBuf[2] = 0x01;
    Usart_Mgr.TXDBuf[3] = report_size;

    memcpy(&Usart_Mgr.TXDBuf[4], report_buf, report_size);
    Usart_Mgr.TXDBuf[4 + report_size] = get_checksum(&Usart_Mgr.TXDBuf[4], report_size);

    uart_repeat_flag = 1;

    UART_Send_Bytes(&Usart_Mgr.TXDBuf[0], report_size + 5);

    uart_repeat_flag = 0;

    wait_us(200);
}

// Drains bytes from the UART RX FIFO into Usart_Mgr.RXDBuf. When a
// full frame has been assembled (state == RX_DONE), calls
// uart_receive_ok() to act on it.
void uart_receive_pro(void) {
    static bool rcv_start = false;

    // Receiving serial data from RF module
    while (uart_available()) {
        rcv_start = true;

        if (Usart_Mgr.RXDLen >= UART_MAX_LEN) {
            uart_read();
        }
        else {
            Usart_Mgr.RXDBuf[Usart_Mgr.RXDLen++] = uart_read();
        }

        if (!uart_available()) {
            wait_us(200);
        }
    }

    // Processing received serial port protocol
    if (rcv_start) {
        rcv_start          = false;
        Usart_Mgr.RXDState = RX_DONE;
        RF_Protocol_Receive();
        Usart_Mgr.RXDLen   = 0;
    }
}

// Initializes USART1 for the RF link (1Mbaud, 8-E-1, half-duplex).
void rf_uart_init(void) {
    /* set uart buad as 460800 */
    uart_init(460800);

    /* Enable parity check */
    USART1->CR1 &= ~((uint32_t)USART_CR1_UE);
    USART1->CR1 |= USART_CR1_M0 | USART_CR1_PCE;
    USART1->CR1 |= USART_CR1_UE;

    /* set Rx and Tx pin pull up */
    GPIOB->OSPEEDR &= ~(GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7);
    GPIOB->PUPDR |= (GPIO_PUPDR_PUPDR6_0 | GPIO_PUPDR_PUPDR7_0);
}

// Sends a handshake to the RF module and waits for its ack. Retries
// up to `timeout` times before giving up and continuing cold.
void rf_device_init(void) {
    uint8_t timeout = 10;


    f_rf_hand_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_HAND, 0, 20);
        wait_ms(5);
        uart_receive_pro(); // receive data
        uart_receive_pro(); // parsing data
        if (f_rf_hand_ok) break;
    }

    timeout           = 10;
    f_rf_read_data_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_READ_DATA, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        uart_receive_pro();
        if (f_rf_read_data_ok) break;
    }

    timeout          = 10;
    f_rf_sts_sysc_ok = 0;
    while (timeout--) {
        uart_send_cmd(CMD_RF_STS_SYSC, 0, 20);
        wait_ms(5);
        uart_receive_pro();
        uart_receive_pro();
        if (f_rf_sts_sysc_ok) break;
    }

    uart_send_cmd(CMD_SET_NAME, 10, 20);

    uart_send_cmd(CMD_SET_24G_NAME, 10, 20);
}

/* ============ RF host driver ============
 * QMK's host_driver_t vtable. Used when link_mode != LINK_USB to route
 * HID reports through the RF UART instead of the native USB stack.
 * Moved here from the old rf_driver.c so all RF code lives in one file.
 */
static uint8_t rf_keyboard_leds(void) {
    return dev_info.rf_led;
}

static void rf_send_keyboard(report_keyboard_t *report) {
    uart_send_report_keyboard(report);
}

static void rf_send_nkro(report_nkro_t *report) {
    uart_send_report_nkro(report);
}

static void rf_send_mouse(report_mouse_t *report) {
    uart_send_mouse_report(report);
}

static void rf_send_extra(report_extra_t *report) {
    if (report->report_id == REPORT_ID_CONSUMER) {
        uart_send_consumer_report(report);
    } else if (report->report_id == REPORT_ID_SYSTEM) {
        uart_send_system_report(report);
    }
}

host_driver_t rf_host_driver = {
    rf_keyboard_leds,
    rf_send_keyboard,
    rf_send_nkro,
    rf_send_mouse,
    rf_send_extra,
};