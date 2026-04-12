#ifndef SYSTICK_HPP
#define SYSTICK_HPP

#include <stdint.h>

void     systick_init(void);
uint32_t systick_get_ms(void);
void     systick_delay_ms(uint32_t ms);

#endif /* SYSTICK_HPP */
