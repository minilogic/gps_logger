/****************************************************************************
    GPS Logger with SD-card.
*****************************************************************************/
#include "driverlib.h"
#include "USB_config/descriptors.h"
#include "USB_API/USB_Common/device.h"
#include "USB_API/USB_Common/usb.h"
#include "USB_API/USB_MSC_API/UsbMscScsi.h"
#include "USB_API/USB_MSC_API/UsbMsc.h"
#include "USB_API/USB_MSC_API/UsbMscStateMachine.h"
#include "USB_app/msc.h"
#include "main.h"
#include "print.h"
#include "hal.h"
#include "gps.h"

void initTimer(void);
void setTimer_A_Parameters(void);

// Global flag by which the timer ISR will trigger main() to check the
// media status
extern uint8_t bDetectCard;
extern struct USBMSC_mediaInfoStr mediaInfo;

Timer_A_initUpModeParam Timer_A_params = {0};

void main (void)
{
    WDT_A_hold(WDT_A_BASE);
    hal_ini_gpio();
    hal_ini_clk();
    #if defined __IAR_SYSTEMS_ICC__
    print(DBG_OUT, "\n\n\n\033[36mGPS Logger (IAR %s %s)\033[0m\n", __DATE__, __TIME__);
    #elif defined __TI_COMPILER_VERSION__
    print(DBG_OUT, "\n\n\n\033[36mGPS Logger (TI %s %s)\033[0m\n", __DATE__, __TIME__);
    #elif defined __GNUC__
    print(DBG_OUT, "\n\n\n\033[36mGPS Logger (GCC %s %s)\033[0m\n", __DATE__, __TIME__);
    #endif
    initTimer();
    __enable_interrupt();       // Enable interrupts globally
    while(1)
    {
        if(hal_switch_state())
        {
            puts_dbg("\033[36mREC Mode\033[0m\n");
            PMM_setVCore(PMM_CORE_LEVEL_1);
            hal_usb_wr(USBPWRCTL, 0);
            gps_enable();
            while(hal_switch_state())
            {
                __bis_SR_register(LPM3_bits + GIE);
                _NOP();
                gps_handler();
                nmea_handler();
            }
            gps_disable();
        }
        else
        {
            while(!hal_switch_state())
            {
                hal_usb_wr(USBPWRCTL, USBDETEN);
                if(!hal_usb_pwr_state())
                {
                    puts_dbg("\033[36mOFF Mode\033[0m\n");
                    PMM_setVCore(PMM_CORE_LEVEL_1);
                    hal_led_off();
                    hal_sd_off();
                    UCS_turnOffXT2();
                    while(!hal_switch_state() && !hal_usb_pwr_state())
                    {
                        __bis_SR_register(LPM3_bits + GIE);
                        _NOP();
                    }
                }
                else
                {
                    puts_dbg("\033[36mUSB Mode\033[0m\n");
                    PMM_setVCore(PMM_CORE_LEVEL_2);
                    hal_sd_on();
                    hal_usb_wr(USBPWRCTL, SLDOEN + VUSBEN + SLDOAON + USBDETEN);
                    USBMSC_initMSC();
                    USB_setup(TRUE, TRUE);
                    while(!hal_switch_state() && hal_usb_pwr_state())
                    {
                        switch(USB_getConnectionState())
                        {
                            case ST_ENUM_ACTIVE:
                                USBMSC_processMSCBuffer();
                                if(bDetectCard)
                                {
                                    USBMSC_checkMSCInsertionRemoval();
                                    if(mediaInfo.mediaChanged) USBMSC_initMSC();
                                }
                                break;
                            case ST_ENUM_SUSPENDED:
                            case ST_PHYS_CONNECTED_NOENUM_SUSP:
                                __bis_SR_register(LPM3_bits + GIE);
                                _NOP();
                                break;
                        }
                        if(bDetectCard)
                        {
                            bDetectCard = 0;
                            if(hal_charge_state()) hal_led_on();
                            else hal_led_inv();
                        }
                    }
                    USB_disable();
                }
                UCS_turnOnXT2(UCS_XT2_DRIVE_8MHZ_16MHZ);
            }
        }
    }
}

/* TIMER0_A0_ISR */
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR (void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) TIMER0_A0_ISR (void)
#else
#error Compiler not found!
#endif
{
    bDetectCard = 1;
    __bic_SR_register_on_exit(LPM3_bits);
}

/* UNMI_ISR */
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector = UNMI_VECTOR
__interrupt void UNMI_ISR (void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(UNMI_VECTOR))) UNMI_ISR (void)
#else
#error Compiler not found!
#endif
{
    switch (__even_in_range(SYSUNIV, SYSUNIV_BUSIFG))
    {
        case SYSUNIV_NONE:
            __no_operation();
            break;
        case SYSUNIV_NMIIFG:
            __no_operation();
            break;
        case SYSUNIV_OFIFG:
            UCS_clearFaultFlag(UCS_XT2OFFG);
            UCS_clearFaultFlag(UCS_DCOFFG);
            SFR_clearInterrupt(SFR_OSCILLATOR_FAULT_INTERRUPT);
            break;
        case SYSUNIV_ACCVIFG:
            __no_operation();
            break;
        case SYSUNIV_BUSIFG:
            // If the CPU accesses USB memory while the USB module is
            // suspended, a "bus error" can occur.  This generates an NMI.  If
            // USB is automatically disconnecting in your software, set a
            // breakpoint here and see if execution hits it.  See the
            // Programmer's Guide for more information.
            SYSBERRIV = 0; //clear bus error flag
            USB_disable(); //Disable
    }
}

/* setTimer_A_Parameters */
void setTimer_A_Parameters()
{
    Timer_A_params.clockSource = TIMER_A_CLOCKSOURCE_ACLK;
    Timer_A_params.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    Timer_A_params.timerPeriod = 32768;
    Timer_A_params.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;
    Timer_A_params.captureCompareInterruptEnable_CCR0_CCIE =
        TIMER_A_CAPTURECOMPARE_INTERRUPT_ENABLE;
    Timer_A_params.timerClear = TIMER_A_DO_CLEAR;
    Timer_A_params.startTimer = false;
}

void initTimer(void)
{
    setTimer_A_Parameters();   
    Timer_A_clearTimerInterrupt(TIMER_A0_BASE);
    Timer_A_initUpMode(TIMER_A0_BASE, &Timer_A_params);
    Timer_A_startCounter(TIMER_A0_BASE, TIMER_A_UP_MODE);
}
