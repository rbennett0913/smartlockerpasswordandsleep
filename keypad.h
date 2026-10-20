#ifndef keypad_h
#define keypad_h

// macros for keypad input values
#define zero_l  335
#define zero_h  352
#define one_l   568
#define one_h   598
#define two_l   622
#define two_h   652
#define three_l 688
#define three_h 718
#define four_l  448
#define four_h  478
#define five_l  482
#define five_h  512
#define six_l   521
#define six_h   551
#define seven_l 374
#define seven_h 394
#define eight_l 397
#define eight_h 417
#define nine_l  423
#define nine_h  443
#define star_l  318
#define star_h  334
#define pound_l 353
#define pound_h 373

/* ---- ADC register definitions (ATmega328P) ---- */

#define ADMUX   (*(volatile unsigned char*)0x7C)
#define ADCSRA  (*(volatile unsigned char*)0x7A)
#define DIDR0   (*(volatile unsigned char*)0x7E)

/* 16-bit access reads ADCL before ADCH, which the hardware requires
 * to keep the pair from the same conversion */
#define ADC     (*(volatile unsigned int*)0x78)

/* ADMUX bits */
#define REFS1   7   /* reference selection, with REFS0 */
#define REFS0   6
#define ADLAR   5   /* 0 -> right-adjusted 10-bit result */
#define MUX3    3   /* input channel selection, with MUX2..0 */
#define MUX2    2
#define MUX1    1
#define MUX0    0

/* ADCSRA bits */
#define ADEN    7   /* ADC enable */
#define ADSC    6   /* start conversion; stays 1 while busy */
#define ADIF    4   /* conversion complete flag */
#define ADIE    3   /* interrupt enable (unused) */
#define ADPS2   2   /* prescaler select, with ADPS1..0 */
#define ADPS1   1
#define ADPS0   0

/* keypad hardware configuration */
#define KEYPAD_CHANNEL 0    /* ADC0 = PC0; change if keypad wired elsewhere */
#define ADC0D   0           /* DIDR0 bit for the ADC0 digital input buffer */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_NONE = 0,
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_STAR,
    KEY_POUND
} key_t;

void adc_init(void);
unsigned int adc_read(void);
key_t keypad_read();

#ifdef __cplusplus
}
#endif

#endif