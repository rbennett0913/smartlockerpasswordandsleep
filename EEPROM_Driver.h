#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include <avr/io.h>

// Low-level write to ATmega328P EEPROM using bare-metal registers
inline void eeprom_write_byte_direct(uint16_t uiAddress, uint8_t ucData) {
    // Wait for completion of previous write
    while (EECR & (1 << EEPE));
    
    // Set up address and data registers
    EEAR = uiAddress;
    EEDR = ucData;
    
    // Write logical one to EEMPE (Master Write Enable)
    EECR |= (1 << EEMPE);
    
    // Start EEPROM write by setting EEPE within 4 clock cycles
    EECR |= (1 << EEPE);
}

// Low-level read from ATmega328P EEPROM using bare-metal registers
inline uint8_t eeprom_read_byte_direct(uint16_t uiAddress) {
    // Wait for completion of previous write
    while (EECR & (1 << EEPE));
    
    // Set up address register
    EEAR = uiAddress;
    
    // Start EEPROM read by writing EERE
    EECR |= (1 << EERE);
    
    // Return data from Data Register
    return EEDR;
}

// EEPROM Addresses
#define ADDR_MAGIC_BYTE   0x0000
#define ADDR_PASS_LEN     0x0001
#define ADDR_PASS_START   0x0002
#define MAGIC_KEY         0xA5

#endif