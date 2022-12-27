#define DBG_OUT 1
#define GPS_OUT 2
#define GSM_OUT 4
#define STR_OUT 8

void putc_dbg(int ch);
void putc_gps(int ch);
void puts_dbg(const char *str);
void puts_gps(const char *str);
void print(int id, ...);
