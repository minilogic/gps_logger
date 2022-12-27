/*
 * ======== hal.c ========
 *
 */
#include "msp430.h"
#include "driverlib.h"
#include "USB_API/USB_Common/device.h"
#include "USB_app/FatFs/HAL_SDCard.h"
#include "USB_config/descriptors.h"
#include "hal.h"

#define GPIO_ALL    GPIO_PIN0|GPIO_PIN1|GPIO_PIN2|GPIO_PIN3| \
                    GPIO_PIN4|GPIO_PIN5|GPIO_PIN6|GPIO_PIN7

const uint8_t p4map[] = { 
    PM_UCB1CLK,
    PM_UCB1SIMO,
    PM_UCB1SOMI,
    PM_UCB1STE,
    PM_UCA1TXD,
    PM_UCA1RXD,
    PM_NONE,
    PM_NONE
};

PMAP_initPortsParam initPortsParam = {
    p4map,
    (uint8_t  *)&P4MAP01,
    1,
    PMAP_DISABLE_RECONFIGURATION
};

/* GPIO config & remap */
void hal_ini_gpio(void)
{
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_ALL);
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_ALL);

    GPIO_setOutputLowOnPin(GPIO_PORT_P2, GPIO_ALL);
    GPIO_setAsOutputPin(GPIO_PORT_P2, GPIO_ALL);

    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_ALL);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_ALL - GPIO_PIN4 - GPIO_PIN5- GPIO_PIN7);
    GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P4, GPIO_PIN4 + GPIO_PIN5);
    GPIO_setAsInputPin(GPIO_PORT_P4, GPIO_PIN7);

    GPIO_setOutputHighOnPin(GPIO_PORT_P5, GPIO_PIN1);
    GPIO_setAsInputPin(GPIO_PORT_P5, GPIO_PIN0);
    GPIO_setAsOutputPin(GPIO_PORT_P5, GPIO_PIN1);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P5,
        GPIO_PIN2 + GPIO_PIN3 + GPIO_PIN4 + GPIO_PIN5);

    GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_ALL - GPIO_PIN0);
    GPIO_setAsOutputPin(GPIO_PORT_P6, GPIO_ALL - GPIO_PIN0);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P6, GPIO_PIN0);

    GPIO_setOutputLowOnPin(GPIO_PORT_PJ, GPIO_ALL);
    GPIO_setAsOutputPin(GPIO_PORT_PJ, GPIO_ALL & ~GPIO_PIN1);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_PJ, GPIO_PIN1);

    PMAP_initPorts(PMAP_CTRL_BASE, &initPortsParam);
}

/* Configures the system clocks:
* MCLK = SMCLK = XT2 = 12MHz, ACLK = XT1 = 32768Hz
*/
bool hal_ini_clk(void)
{
    UCS_setExternalClockSource(32768, 12000000);
    UCS_turnOnLFXT1(UCS_XT1_DRIVE_3, UCS_XCAP_1);
    UCS_turnOnXT2(UCS_XT2_DRIVE_8MHZ_16MHZ);
    UCS_initClockSignal(UCS_ACLK, UCS_XT1CLK_SELECT, UCS_CLOCK_DIVIDER_1);
    UCS_initClockSignal(UCS_MCLK, UCS_XT2CLK_SELECT, UCS_CLOCK_DIVIDER_1);
    UCS_initClockSignal(UCS_SMCLK, UCS_XT2CLK_SELECT, UCS_CLOCK_DIVIDER_1);
    HWREG8(UCS_BASE + OFS_UCSCTL1) = DISMOD;
    return 1;
}

void hal_sd_off(void)
{
    GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_PJ, GPIO_PIN1);
    GPIO_setOutputLowOnPin(GPIO_PORT_P4, GPIO_PIN6);
    SDCard_deinit();
}

void hal_sd_on(void)
{
    GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN6);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_PJ, GPIO_PIN1);
}

void hal_usb_wr(uint16_t reg, uint16_t dat)
{
    USBKEYPID = 0x9628;
    HWREG16(reg) = dat;
    USBKEYPID = 0x9600;
}


void hal_gps_enable(void)
{
    USCI_A_UART_initParam gps_uart = {0};
    GPIO_setOutputHighOnPin(GPIO_PORT_P6, GPIO_PIN3);
  GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P4, GPIO_PIN5);
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P4, GPIO_PIN5);
    GPIO_setAsPeripheralModuleFunctionOutputPin(GPIO_PORT_P4, GPIO_PIN4);
    #if 0
    // 115200 bps
    gps_uart.selectClockSource = USCI_A_UART_CLOCKSOURCE_SMCLK;
    gps_uart.clockPrescalar = 6;
    gps_uart.firstModReg = 8;
    gps_uart.secondModReg = 0;
    gps_uart.overSampling = USCI_A_UART_OVERSAMPLING_BAUDRATE_GENERATION;
    #else
    // 9600 bps
    gps_uart.selectClockSource = USCI_A_UART_CLOCKSOURCE_ACLK;
    gps_uart.clockPrescalar = 3;
    gps_uart.firstModReg = 0;
    gps_uart.secondModReg = 3;
    gps_uart.overSampling = USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION;
    #endif
    gps_uart.parity = USCI_A_UART_NO_PARITY;
    gps_uart.msborLsbFirst = USCI_A_UART_LSB_FIRST;
    gps_uart.numberofStopBits = USCI_A_UART_ONE_STOP_BIT;
    gps_uart.uartMode = USCI_A_UART_MODE;
    USCI_A_UART_init(USCI_A1_BASE, &gps_uart);
    USCI_A_UART_enable(USCI_A1_BASE);
    USCI_A_UART_clearInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
    USCI_A_UART_enableInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
}

void hal_gps_disable(void)
{
    USCI_A_UART_disable(USCI_A1_BASE);
    USCI_A_UART_disableInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
    GPIO_setAsInputPinWithPullDownResistor(GPIO_PORT_P4, GPIO_PIN4 + GPIO_PIN5);
    GPIO_setOutputLowOnPin(GPIO_PORT_P6, GPIO_PIN3);
}

/*
void hal_gps_reinit(void)
{
    USCI_A_UART_initParam gps_uart = {0};
//    USCI_A_UART_disable(USCI_A1_BASE);
//    USCI_A_UART_disableInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
    gps_uart.selectClockSource = USCI_A_UART_CLOCKSOURCE_ACLK;
    gps_uart.clockPrescalar = 3;
    gps_uart.firstModReg = 0;
    gps_uart.secondModReg = 3;
    gps_uart.overSampling = USCI_A_UART_LOW_FREQUENCY_BAUDRATE_GENERATION;
    gps_uart.parity = USCI_A_UART_NO_PARITY;
    gps_uart.msborLsbFirst = USCI_A_UART_LSB_FIRST;
    gps_uart.numberofStopBits = USCI_A_UART_ONE_STOP_BIT;
    gps_uart.uartMode = USCI_A_UART_MODE;
    USCI_A_UART_init(USCI_A1_BASE, &gps_uart);
//    USCI_A_UART_enable(USCI_A1_BASE);
//    USCI_A_UART_clearInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
//    USCI_A_UART_enableInterrupt(USCI_A1_BASE, USCI_A_UART_RECEIVE_INTERRUPT);
}
*/
