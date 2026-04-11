#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdint.h>

void     systick_init(void);
uint32_t systick_get_ms(void);
void     systick_delay_ms(uint32_t ms);

#endif /* SYSTICK_H */