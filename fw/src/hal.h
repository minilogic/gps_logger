/*
 * ======== hal.h ========
 *
 * Device and board specific pins need to be configured here
 *
 */


#define hal_led_on() GPIO_setOutputLowOnPin(GPIO_PORT_PJ, GPIO_PIN2)
#define hal_led_off() GPIO_setOutputHighOnPin(GPIO_PORT_PJ, GPIO_PIN2)
#define hal_led_inv() GPIO_toggleOutputOnPin(GPIO_PORT_PJ, GPIO_PIN2)
#define hal_switch_state() GPIO_getInputPinValue(GPIO_PORT_P4, GPIO_PIN7)
#define hal_charge_state() GPIO_getInputPinValue(GPIO_PORT_P6, GPIO_PIN0)
#define hal_usb_pwr_state() (USBPWRCTL & USBBGVBV)
 
bool hal_ini_clk(void);
void hal_ini_gpio(void);
void hal_usb_wr(uint16_t reg, uint16_t dat);
void hal_sd_off(void);
void hal_sd_on(void);

void hal_gps_enable(void);
void hal_gps_disable(void);

void hal_gps_reinit(void);
