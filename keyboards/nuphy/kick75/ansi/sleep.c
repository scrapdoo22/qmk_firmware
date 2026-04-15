#include "ansi.h"
#include "hal_usb.h"
#include "usb_main.h"

extern user_config_t    user_config;
extern DEV_INFO_STRUCT      dev_info;
extern bool                 f_wakeup_prepare;
extern bool                 f_goto_sleep;
extern uint16_t             rf_linking_time;
extern uint16_t             no_act_time;

uint8_t uart_send_cmd(uint8_t cmd, uint8_t ack_cnt, uint8_t delayms);

// Drives the auto-sleep state machine. On USB, sleeps after 1s of USB
// suspend. On RF, sleeps after SLEEP_TIME_DELAY of no key activity,
// or immediately if the RF link goes idle/disconnected. On wakeup,
// re-runs the RF handshake and clears held keys.
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
                extern void m_break_all_key(void);
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