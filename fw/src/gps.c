#include <string.h>
#include "driverlib.h"
#include "hal.h"
#include "print.h"
#include "usb_app/FatFs/ff.h"
#include "usb_app/FatFs/diskio.h"
#include "usb_app/FatFs/HAL_SDCard.h"



char buf[2][512];
uint16_t buf_idx;
uint8_t  buf_num;
volatile uint8_t stat;
FATFS FatFs;
FIL file;

char file_name[] = "nmea/log000.txt";

void gps_enable(void)
{
    FRESULT rc;
    hal_led_off();
    hal_sd_on();
    puts_dbg("sd init\n");
    SDCard_init();
    disk_initialize(0);     //Attempt to initialize it
    f_mount(&FatFs, "", 0);
    rc = f_mkdir("nmea");
    print(DBG_OUT, "mkdir:%u\n", rc);
//    rc = f_open(&file, file_name, FA_WRITE | FA_CREATE_NEW);
//    print(DBG_OUT, "fopen:%u\n", rc);

    for(uint16_t file_number = 0 ; file_number < 999 ; file_number++ )
    {
        file_name[10] = file_number % 10 + '0';
        file_name[9] = file_number /10 % 10 + '0';
        file_name[8] = file_number /100 % 10 + '0';
        rc = f_open(&file, file_name, FA_WRITE | FA_CREATE_NEW);
        if(rc == FR_OK)
        {
            print(DBG_OUT, "fopen: log%u\n", file_number);
            break;
        }
	}

    buf_idx = buf_num = stat = 0;
    hal_gps_enable();


    #if 0
    while(!(stat & 1));
    stat &= ~1;
    puts_gps("$PSIMIPR,W,9600*14\r\n");
//    while(!(stat & 1));
//    stat &= ~1;
    hal_gps_reinit();
    #endif


    while(!(stat & 1));
    stat &= ~1;
    puts_gps("$PMTK314,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28\r\n");



}

void gps_disable(void)
{
    //FRESULT rc;
	UINT bw;
    hal_gps_disable();
    if(buf_idx)
    {
        //rc =
        f_write(&file, &buf[buf_num], buf_idx, &bw);
    }
    //rc = 
    f_close(&file);
    f_mount(0, "", 0);
}

void gps_handler(void)
{
    //FRESULT rc;
	UINT bw;
    if(stat & 2)
    {
        stat &= ~2;
        hal_led_on();
        //rc = 
        f_write(&file, &buf[1 - buf_num], sizeof(buf[0]), &bw);
        hal_led_off();
    }
}

/* USCI_A1_ISR */
#if defined(__TI_COMPILER_VERSION__) || (__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__) && (__MSP430__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR (void)
#else
#error Compiler not found!
#endif
{
    uint8_t c;
    c = UCA1RXBUF;
    buf[buf_num][buf_idx] = c;
    if(c == '\n') stat |= 1;    // есть новое сообщение
//    else putc_dbg(c);
    if(++buf_idx == sizeof(buf[0]))
    {
        buf_idx = 0;
        if(++buf_num == (sizeof(buf) / sizeof(buf[0]))) buf_num = 0;
        stat |= 2;              // есть новый сектор данных
        __bic_SR_register_on_exit(LPM3_bits);
    }
}

/*
#define UART_BUF_SIZE (1 << 9)
uint8_t uart_buffer[UART_BUF_SIZE];
uint16_t uart_head, uart_tail;
#define uart_inc_ptr(ptr) { ptr++; ptr &= (UART_BUF_SIZE - 1); }


void isr_uart(void)
{
    uart_buffer[uart_head] = UCA1RXBUF;
    if(++uart_head == sizeof(uart_buffer)) uart_head = 0;
//    uart_inc_ptr(uart_head);
    if(uart_head == uart_tail) uart_inc_ptr(uart_tail);
    //__bic_SR_register_on_exit(LPM3_bits);
}

int16_t get_uart(void)
{
    int16_t res = -1;
    if(uart_tail != uart_head)
    {
        __disable_interrupt();
        res = uart_buffer[uart_tail];
        uart_inc_ptr(uart_tail);
        __enable_interrupt();
    }
    return res;
}
*/

char nmea[128];
uint8_t state;
//uint8_t nmea[128], state;
volatile uint8_t *rx_head, *rx_tail;
#define RXBUF OFS_USBSTABUFF
#define RXBUF_SIZE 2048
#define ini_ptr(ptr) ptr = (uint8_t *)RXBUF
#define inc_ptr(ptr) if((uint16_t)++ptr == RXBUF + RXBUF_SIZE) ini_ptr(ptr)
const char hex[] = "0123456789ABCDEF";




void nmea_isr(void)
{
    *rx_head = UCA1RXBUF;
    inc_ptr(rx_head);
    if(rx_head == rx_tail) inc_ptr(rx_tail);
    //__bic_SR_register_on_exit(LPM3_bits);
}

static int gps_message(void)
{
    char c;
    static uint8_t checksum, index;
    if(rx_head != rx_tail)
    {
        __disable_interrupt();
        c = *rx_tail;
        inc_ptr(rx_tail);
        __enable_interrupt();

        if(state == 1)
        {
            if(c == '*')
            {
                nmea[index] = 0;
                state++;
            }
            else
            {
                checksum ^= c;
                nmea[index++] = c;
                if(index == sizeof(nmea)) state = 0;
            }
        }
        else if(state == 2)
        {
            if(c == hex[checksum >> 4]) state++;
            else state = 0;
        }
        else if(state == 3)
        {
            if(c == hex[checksum & 15]) state++;
            else state = 0;
        }
        else if(state == 4)
        {
            if(c == '\r') state++;
            else state = 0;
        }
        else if(state == 5)
        {
            state = 0;
            if(c == '\n') return 1;
        }
        else if(c == '$')
        {
            checksum = index = 0;
            state = 1;
        }
    }
    return 0;
}


void nmea_init(void)
{
    ini_ptr(rx_head);
    ini_ptr(rx_tail);
    state = 0;
    while(!gps_message());
    puts_gps("PMTK314,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0");
//    while(!nmea_msg());
}





struct KML {
    char time[10];
    char date[10];
    char latitude[12];
    char longitude[12];
    char speed[5];
};



char *nmea_field(uint16_t ctr)
{
    char *p = nmea;
    while(ctr--) do if(*p == 0) return NULL; while(*p++ != ',');
    return p;
}

enum RMC { RMC_ID, RMC_Time, RMC_Status, RMC_Latitude, RMC_NS, RMC_Longitude,
           RMC_EW, RMC_Speed, RMC_Course, RMC_Date, RMC_Mode };

int set_kml_latitude(uint16_t field, char *kml_latitude)
{
    char *p;
    p = nmea_field(field + 1);
    if(p == NULL || *p != 'N' || *p != 'S') return 0;
    if(*p == 'S') *kml_latitude++ = '-';
    p = nmea_field(field);
    if(p == NULL) return 0;
    *kml_latitude++ = *p++;
    *kml_latitude++ = *p++;

    return 1;
}

int set_kml_longitude(uint16_t field, char *kml_latitude)
{
    char *p;
    p = nmea_field(field + 1);
    if(p == NULL || *p != 'N' || *p != 'S') return 0;
    if(*p == 'S') *kml_latitude++ = '-';
    p = nmea_field(field);
    if(p == NULL) return 0;
    *kml_latitude++ = *p++;
    *kml_latitude++ = *p++;

    return 1;
}

int set_kml_tm(char time_field, char date_field, char *kml_tm)
{
    return 1;
}

static int nmea_rmc_parse(struct KML *kml)
{
    char *p;
    if(nmea[2] != 'R' && nmea[3] != 'M' && nmea[4] != 'C') return 0;
    p = nmea_field(RMC_Status);
    if(p == NULL || *p != 'A') return 0;
    //p = nmea_field(RMC_Time);
    //if(p == NULL) return 0;
    //kml->time[0] = *p++;
    //kml->time[0] = *p++;

    if(set_kml_latitude(RMC_Latitude, kml->latitude) ||
       set_kml_longitude(RMC_Longitude, kml->longitude)) return 0;





//    if(*p == 'R' && *++p == 'M' && *++p == 'C' && *++p == ',')
    if(nmea[2] != 'R' && nmea[3] != 'M' && nmea[4] != 'C' && nmea[5] != ',' &&
       nmea[12] != '.' && nmea[16] != ',' && nmea[17] != 'A' && nmea[18] != ',')
        return 0;
    // get latitude
    p = kml->latitude;
    if(nmea[29] == 'S') *p++ = '-';
    else if(nmea[29] != 'N') return 0;
    *p++ = nmea[19];
    *p++ = nmea[20];
    if(nmea[21] != '.') return 0;
    *p++ = '.';

    return 1;
}



void nmea_handler(void)
{
    struct KML kml;
    if(gps_message())
    {
        if(nmea[0] == 'G' && (nmea[1] == 'P' || nmea[1] == 'N'))
        {
            if(nmea_rmc_parse(&kml))
            {
                puts_dbg("rmc\n");
            }
        }


/*
        if(nmea[0] == 'G' && nmea[5] == ',')
        {
            if(nmea[2] == 'R' && nmea[3] == 'M' && nmea[4] == 'C')
            {
                if(nmea_rmc_parse())
                {
                }
            }
            else if(nmea[2] == 'G' && nmea[3] == 'S' && nmea[4] == 'A')
            {
                if(nmea[1] == 'P')
                {
                    puts_dbg("3rmc\n");
                }
                else if(nmea[1] == 'P')
                {
                    puts_dbg("4rmc\n");
                }

            }
        }
        if(strcmp(nmea, "GPRMC,") == 0)
        {
            puts_dbg("gps\n");
        }
        else if(strcmp(&nmea[2], "GNRMC,") == 0)
        {
            puts_dbg("gns\n");
        }
        else if(strcmp(nmea, "GPGSA,") == 0)
        {
            puts_dbg("gns\n");
        }
        else if(strcmp(nmea, "GNGSA,") == 0)
        {
            puts_dbg("gns\n");
        }
*/
    }
}








/*
int16_t nmea_get_char(void)
{
    int16_t res = -1;
    if(head != tail)
    {
        __disable_interrupt();
        res = *tail;
        inc_ptr(tail);
        __enable_interrupt();
    }
    return res;
}

void uart(void)
{
    static uint8_t nmea[96], checksum, index = 0, state = 0;
    uint8_t c = UCA1RXBUF;



    if(c == '$') index = 0;
    else if(c == '\n')
    {
        if(nmea[--index] == '\r')
        {
            checksum ^= nmea[index];

            if(nmea[--index]
        }
    }
    else
    {
        checksum ^= c;
        nmea[index] = c;
        if(++index == sizeof(nmea)) index = 0;
    }




    if(index == 0)
    {
        if(c == '$') index++;
    }
    else
    {


        if(c == '\n')
        {
            index = 0;
        }
        else
        {
//            checksum ^= c;
            nmea[index] = c;
            if(++index == sizeof(nmea)) index = 0;
        }
    }




    if(state == 1)
    {
        if(c == '*') state++;
        else if(c >= ',' && c <= 'Z')
        {
            checksum ^= c;
            nmea[index] = c;
            if(++index == sizeof(nmea)) state = 0;
        }
        else state = 0;
    }
    else if(state == 2)
    {
        if((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
        {
            state++;
        }
        else state = 0;
    }
    else if(state == 3)
    {
        if((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))
        {
            state++;
        }
        else state = 0;
    }
    else if(state == 4)
    {
        if(c == '\r') state++;
        else state = 0;
    }
    else if(state == 5)
    {
        state = 0;
        if(c == '\n')
        {
        }
    }
    else if(c == '$')
    {
        index = checksum = 0;
        state = 1;
    }
}
*/
