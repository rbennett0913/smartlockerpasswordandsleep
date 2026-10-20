#ifndef locker_h
#define locker_h

#include "keypad.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LOCKER_IN_PROGRESS, LOCKER_MATCH, LOCKER_WRONG } locker_status_t;

locker_status_t locker_check(void);
void locker_reset(void);
void locker_add_digit(key_t key);

#ifdef __cplusplus
}
#endif

#endif