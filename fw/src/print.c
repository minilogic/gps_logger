#include <stdarg.h>
#include "driverlib.h"
#include "print.h"

uint8_t stream_id;
char * stream_ptr;
const uint8_t bm[8] = { 1,2,4,8,16,32,64,128 };

void putc_dbg(int ch)
{
    uint8_t tmp = PJOUT;
    const uint8_t *ptr = &bm[0];
    __asm(
        "push   SR\n"
        "dint\n"            // disable all interrupts
        "nop\n"
        "bic.b  #0x01,%3\n" // start
        "bit.b  @%2+,%0\n"  // b0   2+1+1+3
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b1
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b2
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b3
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b4
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b5
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b6
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "bit.b  @%2+,%0\n"  // b7
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "setc\n"            // stop 2+1+1+3
        "bic.b  #0x01,%1\n"
        "addc.b #0x00,%1\n"
        "nop\n"
        "mov.b  %1,%3\n"
        "pop    SR\n"       // Restore interrupt state
        "nop\n"
        :: "r"(ch), "r"(tmp), "r"(ptr), "m"(PJOUT));
}

void putc_gps(int ch)
{
    UCA1TXBUF = ch;
    while(UCA1STAT & UCBUSY);
}

void puts_dbg(const char *str)
{
    while(*str) putc_dbg(*str++);
}

void puts_gps(const char *str)
{
    while(*str) putc_gps(*str++);
/*
    uint8_t cksum;
    putc_gps('$');
    cksum = 0;
    while(*str)
    {
        putc_gps(*str);
        cksum ^= *str;
        str++;
    }
    putc_gps('*');
    putc_gps(cksum);
    putc_gps(cksum);
*/
}

static void putc(int ch)
{
    if(stream_id & DBG_OUT) putc_dbg(ch);
    if(stream_id & GPS_OUT) putc_gps(ch);
    if(stream_id & STR_OUT) *stream_ptr++ = ch;
}

static void puts(const char *str)
{
    while(*str) putc(*str++);
}

static void vprintf(const char *fmt, va_list arp)
{
    unsigned long v, r;
    char s[10], c, f;
    while(1)
    {
        if(!(c = *fmt++)) break;
        if(c != '%') putc(c);
        else
        {
            f = 0;
            c = *fmt++;
            if(c == 'l')
            {
                f++;
                c = *fmt++;
            }
            if(!c) break;
            switch(c)
            {
                case 's':
                    puts(va_arg(arp, const char *));
                    continue;
                case 'x':
                    r = 16;
                    break;
                case 'd':
                case 'u':
                    r = 10;
                    break;
                case 'c':
                    c = (char)va_arg(arp, int);
                default:
                    putc(c);
                    continue;
            }
            v = f ? va_arg(arp, unsigned long) : 
                    (unsigned long)va_arg(arp, unsigned int);
            if(c == 'd')
            {
                if(!f && (int)v < 0) v |= 0xFFFF0000;
                if((long)v < 0)
                {
                    putc('-');
                    v = (unsigned long)-(long)v;
                }
            }
            f = 0;
            do
            {
                c = (char)(v % r);
                if(c > 9) c += 7;
                c += '0';
                s[f++] = c;
                v /= r;
            } while(v);
            do putc(s[--f]); while(f);
        }
    }
}

void print(int id, ...)
{
    va_list arg;
    va_start(arg, id);
    stream_id = id;
    if(stream_id & STR_OUT) stream_ptr = va_arg(arg, char *);
    vprintf(va_arg(arg, const char *), arg);
    va_end(arg);
    if(stream_id & STR_OUT) *stream_ptr = 0;
}
