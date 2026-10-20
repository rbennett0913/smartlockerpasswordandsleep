// defines registers/pins from datasheet for use in main program
// unsigned char -> 8 bits
// volatile because the hardware can overwrite the value

#ifndef pwm_h
#define pwm_h

#define DDRB (*(volatile unsigned char*)0x24)
#define PB1 1

#define TCCR1A (*(volatile unsigned char*)0x80)
#define COM1A1 7
#define COM1A0 6
#define WGM11 1

#define TCCR1B (*(volatile unsigned char*)0x81)
#define WGM13 4
#define WGM12 3
#define CS11 1

#define ICR1H (*(volatile unsigned char*)0x87)
#define ICR1L (*(volatile unsigned char*)0x86)

#define OCR1AH (*(volatile unsigned char*)0x89)
#define OCR1AL (*(volatile unsigned char*)0x88)

#define TIFR1 (*(volatile unsigned char*)0x36)
#define TOV1 0

#ifdef __cplusplus
extern "C" {
#endif

unsigned long milliseconds(void);
void tick_update(void);
void pwm_init(void);
void setPosUnlock(void);
void setPosLock(void);

#ifdef __cplusplus
}
#endif

#endif