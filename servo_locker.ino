#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "EEPROM_Driver.h"
#include "keypad.h"
#include "pwm.h"
#include "locker.h"
#include "fsm.h"

/* ================================================================= *
 * 1. PWM & SERVO IMPLEMENTATION                                     *
 * ================================================================= */
#define TICK_MS 20UL
static volatile unsigned long g_ms = 0;

unsigned long milliseconds(void) {
    return g_ms;
}

void tick_update(void) {
    if (TIFR1 & (1 << TOV1)) {  
        TIFR1 = (1 << TOV1);    
        g_ms += TICK_MS;
    }
}

void pwm_init(void) {
    DDRB |= (1 << PB1); // OC1A (PB1 / Arduino D9) as output
    TCCR1A = (1 << COM1A1) | (1 << WGM11);
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);

    ICR1H = (unsigned char)(39999 >> 8);
    ICR1L = (unsigned char)(39999 & 0xFF);
}

static void setPulse(unsigned int counts) {
    OCR1AH = (unsigned char)(counts >> 8);
    OCR1AL = (unsigned char)(counts & 0xFF);
}

void setPosUnlock(void) { setPulse(1999); }   /* 1.0 ms -> unlocked */
void setPosLock(void)   { setPulse(3999); }   /* 2.0 ms -> locked   */

/* ================================================================= *
 * 2. KEYPAD & ADC IMPLEMENTATION                                    *
 * ================================================================= */
void adc_init(void) {
    ADMUX  = (1 << REFS0) | KEYPAD_CHANNEL; 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    (void)adc_read(); // Dummy conversion
    
    // Keep digital input buffer ENABLED so pin-change interrupts work for ALL key voltages
    DIDR0 &= ~(1 << ADC0D); 
}

unsigned int adc_read(void) {
    ADCSRA |= (1 << ADSC);              
    while (ADCSRA & (1 << ADSC)) { }   
    return ADC;
}

typedef struct {
    unsigned int lo;
    unsigned int hi;
    key_t        key;
} key_range_t;

static const key_range_t keymap[] = {
    { zero_l,  zero_h,  KEY_0 },
    { one_l,   one_h,   KEY_1 },
    { two_l,   two_h,   KEY_2 },
    { three_l, three_h, KEY_3 },
    { four_l,  four_h,  KEY_4 },
    { five_l,  five_h,  KEY_5 },
    { six_l,   six_h,   KEY_6 },
    { seven_l, seven_h, KEY_7 },
    { eight_l, eight_h, KEY_8 },
    { nine_l,  nine_h,  KEY_9 },
    { star_l,  star_h,  KEY_STAR },
    { pound_l, pound_h, KEY_POUND },
};

#define DEBOUNCE_MS 350UL
static key_t prev_raw = KEY_NONE;
static unsigned long last_edge_ms = 0UL - DEBOUNCE_MS;

key_t keypad_read() {
    unsigned char i;
    unsigned int adc = adc_read();    
    key_t raw = KEY_NONE;

    for (i = 0; i < sizeof keymap / sizeof keymap[0]; ++i) {
        if (adc >= keymap[i].lo && adc <= keymap[i].hi) {
            raw = keymap[i].key;
            break;
        }
    }

    if (raw != KEY_NONE && prev_raw == KEY_NONE
        && (milliseconds() - last_edge_ms) >= DEBOUNCE_MS) {
        prev_raw = raw;
        last_edge_ms = milliseconds();
        return raw;                   
    }

    prev_raw = raw;
    return KEY_NONE;
}

/* ================================================================= *
 * 3. LOCKER PASSCODE BUFFER & EEPROM STORAGE                        *
 * ================================================================= */
#define PIN_LEN 4
static key_t attempt[PIN_LEN];
static unsigned int len = 0;

char savedPassword[PIN_LEN + 1];

void locker_reset(void) {
    len = 0;
}

void locker_add_digit(key_t key) {
    if (len == PIN_LEN) {
        len = 0;
    }
    attempt[len++] = key;
}

void load_password_from_eeprom(void) {
    uint8_t magic = eeprom_read_byte_direct(ADDR_MAGIC_BYTE);
    if (magic != MAGIC_KEY) {
        eeprom_write_byte_direct(ADDR_PASS_LEN, 4);
        eeprom_write_byte_direct(ADDR_PASS_START + 0, '1');
        eeprom_write_byte_direct(ADDR_PASS_START + 1, '2');
        eeprom_write_byte_direct(ADDR_PASS_START + 2, '3');
        eeprom_write_byte_direct(ADDR_PASS_START + 3, '4');
        eeprom_write_byte_direct(ADDR_MAGIC_BYTE, MAGIC_KEY);
    }
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        savedPassword[i] = (char)eeprom_read_byte_direct(ADDR_PASS_START + i);
    }
    savedPassword[PIN_LEN] = '\0';
}

void save_password_to_eeprom(const char* newPass) {
    for (uint8_t i = 0; i < PIN_LEN; i++) {
        eeprom_write_byte_direct(ADDR_PASS_START + i, newPass[i]);
        savedPassword[i] = newPass[i];
    }
    savedPassword[PIN_LEN] = '\0';
    eeprom_write_byte_direct(ADDR_PASS_LEN, PIN_LEN);
}

locker_status_t locker_check(void) {
    unsigned int i;
    if (len != PIN_LEN) {
        return LOCKER_IN_PROGRESS;   
    }

    char attemptStr[PIN_LEN + 1];
    for (i = 0; i < PIN_LEN; i++) {
        if (attempt[i] >= KEY_0 && attempt[i] <= KEY_9) {
            attemptStr[i] = '0' + (attempt[i] - KEY_0);
        } else {
            attemptStr[i] = '?';
        }
    }
    attemptStr[PIN_LEN] = '\0';

    locker_reset();                  
    if (strcmp(attemptStr, savedPassword) == 0) {
        return LOCKER_MATCH;
    }
    return LOCKER_WRONG;
}

/* ================================================================= *
 * 4. FSM & POWER-DOWN SLEEP LOGIC                                   *
 * ================================================================= */
typedef enum {
  STATE_LOCKED,
  STATE_UNLOCKED,
  STATE_LOCKING,
  STATE_REPROGRAM,
  STATE_SLEEP
} state_t;

static state_t state = STATE_LOCKED;
static state_t prev_state;  

static unsigned long last_activity;  
static char reprogramBuf[PIN_LEN + 1];
static uint8_t reprogramIdx = 0;

ISR(PCINT1_vect) {
    // Empty ISR to catch wake-up trigger from any key press on Analog Pin A0
}

void setup_sleep_interrupts(void) {
    PCICR |= (1 << PCIE1);   // Enable PCIE1 for PORTC (Analog pins A0-A5)
    PCMSK1 |= (1 << PCINT8); // Unmask PCINT8 (corresponds to PC0 / A0)
}

static void handle_locked(key_t key, unsigned long now) {
    if (key == KEY_NONE) return;
    
    if (key == KEY_STAR) {
        locker_status_t status = locker_check();
        switch (status) {
            case LOCKER_MATCH:
                setPosUnlock();
                Serial.println(F("Access Granted! Unlocking..."));
                Serial.println(F("Status: UNLOCKED. Press '*' to lock, '#' to reprogram."));
                state = STATE_UNLOCKED;
                break;
            case LOCKER_WRONG:
                Serial.println(F("Access Denied! Incorrect Passcode."));
                break;
            case LOCKER_IN_PROGRESS:
            default:
                Serial.println(F("Incomplete Entry! Enter 4 digits before '*'."));
                locker_reset();
                break;
        }
    } 
    else if (key == KEY_POUND) {
        Serial.println(F("Cleared Entry."));
        locker_reset();
    }
    else if (key >= KEY_0 && key <= KEY_9) {
        locker_add_digit(key);
        Serial.println(F("Digit entered."));
    }
}

static void handle_unlocked(key_t key, unsigned long now) {
    if (key == KEY_STAR) {
        setPosLock();
        Serial.println(F("Locking system..."));
        state = STATE_LOCKED;
        Serial.println(F("Status: LOCKED. Enter passcode:"));
    } else if (key == KEY_POUND) {
        state = STATE_REPROGRAM;
        reprogramIdx = 0;
        Serial.println(F("Enter NEW Passcode (press '*' when done):"));
    }
}

static void handle_reprogram(key_t key, unsigned long now) {
    if (key == KEY_NONE) return;
    
    if (key == KEY_POUND) {
        reprogramIdx = 0;
        Serial.println(F("Cleared New Passcode Entry."));
    } else if (key == KEY_STAR) {
        if (reprogramIdx > 0) {
            reprogramBuf[reprogramIdx] = '\0';
            save_password_to_eeprom(reprogramBuf);
            Serial.print(F("New Passcode Saved to EEPROM: "));
            Serial.println(savedPassword);
            Serial.println(F("Locking system with new passcode..."));
            setPosLock();
            state = STATE_LOCKED;
            Serial.println(F("Status: LOCKED. Enter passcode:"));
        } else {
            Serial.println(F("Cannot save empty password!"));
        }
        reprogramIdx = 0;
    } else if (key >= KEY_0 && key <= KEY_9) {
        if (reprogramIdx < PIN_LEN) {
            reprogramBuf[reprogramIdx++] = '0' + (key - KEY_0);
            reprogramBuf[reprogramIdx] = '\0';
            Serial.print(F("New Buffer: "));
            Serial.println(reprogramBuf);
        }
    }
}

static void handle_locking(key_t key, unsigned long now) {
  state = STATE_LOCKED;  
}

static void handle_sleep(key_t key, unsigned long now) {
    if (key != KEY_NONE) {
        state = prev_state;
        Serial.println(F("Woke up from sleep."));
        return;
    }

    // Ensure the "Entering Power-Down Sleep Mode..." message fully prints before halting CPU
    Serial.flush();

    // Enter Power-Down Mode
    ADCSRA &= ~(1 << ADEN); 
    SMCR = (1 << SM1) | (1 << SE); 

    sei(); 
    __asm__ __volatile__ ("sleep" ::: "memory");

    // --- WAKE UP ---
    SMCR &= ~(1 << SE);     
    ADCSRA |= (1 << ADEN);  
    
    // Perform dummy ADC conversion to re-initialize internal ADC circuitry after sleep
    (void)adc_read(); 

    last_activity = milliseconds();
}

void fsm_init(void) {
    pwm_init();
    adc_init();
    load_password_from_eeprom();
    setup_sleep_interrupts();
    setPosLock(); 
    last_activity = milliseconds();
}

void fsm_tick(key_t key) {
    unsigned long now = milliseconds();

    // 15-second inactivity timeout -> enter deep sleep
    if (key != KEY_NONE) {
        last_activity = now;
    } else if (state != STATE_SLEEP && now - last_activity >= 15000UL) {
        prev_state = state;
        state = STATE_SLEEP;
        locker_reset();  
        Serial.println(F("Entering Power-Down Sleep Mode..."));
    }

    switch (state) {
        case STATE_LOCKED:    handle_locked(key, now);    break;
        case STATE_UNLOCKED:  handle_unlocked(key, now);  break;
        case STATE_LOCKING:   handle_locking(key, now);   break;
        case STATE_REPROGRAM: handle_reprogram(key, now); break;
        case STATE_SLEEP:     handle_sleep(key, now);     break;
    }
}

/* ================================================================= *
 * 5. MAIN ARDUINO SETUP & LOOP                                      *
 * ================================================================= */
void setup() {
    Serial.begin(9600);
    fsm_init();
    
    Serial.println(F("\n=== Personal Smart Locker Initialized ==="));
    Serial.print(F("Loaded Stored Passcode: "));
    Serial.println(savedPassword);
    Serial.println(F("Status: LOCKED. Enter passcode:"));
}

void loop() {
    tick_update();

    key_t key = keypad_read();

    switch (key) {
        case KEY_0:    Serial.println(F("Key Pressed: 0")); break;
        case KEY_1:    Serial.println(F("Key Pressed: 1")); break;
        case KEY_2:    Serial.println(F("Key Pressed: 2")); break;
        case KEY_3:    Serial.println(F("Key Pressed: 3")); break;
        case KEY_4:    Serial.println(F("Key Pressed: 4")); break;
        case KEY_5:    Serial.println(F("Key Pressed: 5")); break;
        case KEY_6:    Serial.println(F("Key Pressed: 6")); break;
        case KEY_7:    Serial.println(F("Key Pressed: 7")); break;
        case KEY_8:    Serial.println(F("Key Pressed: 8")); break;
        case KEY_9:    Serial.println(F("Key Pressed: 9")); break;
        case KEY_STAR: Serial.println(F("Key Pressed: *")); break;
        case KEY_POUND:Serial.println(F("Key Pressed: #")); break;
        default: break;
    }

    fsm_tick(key);
}