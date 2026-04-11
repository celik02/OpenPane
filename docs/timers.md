# Timer Reference — STM32F446RE

## SysTick vs General-Purpose TIM

| | SysTick | General-purpose TIM (e.g. TIM2) |
|---|---|---|
| Purpose | OS tick / simple delay | Precise, multi-channel timing |
| Resolution | Fixed to HCLK or HCLK/8 | Prescaler + ARR, very flexible |
| Channels | None | 4 capture/compare channels |
| PWM output | No | Yes |
| One-shot mode | No | Yes |
| IRQ priority | Configurable | Configurable |
| Cost | Always present (Cortex-M core) | Peripheral clock must be enabled |

## When to use which

**SysTick** — simple 1 ms system tick, `delay_ms()`, uptime counter, task scheduling heartbeat.

**TIM** — PWM generation (backlight, buzzer), input capture (pulse width measurement), precise one-shot delays, multiple independent timeouts running concurrently.

## Recommended pattern for this project

- **SysTick** → 1 ms tick, `systick_get_ms()`, `systick_delay_ms()`
- **TIM** → reserved for PWM or future peripherals (e.g. display backlight, buzzer)

## SysTick at 16 MHz (HSI, no PLL)

Reload value = `SystemCoreClock / 1000 - 1` = `15999`

CMSIS helper: `SysTick_Config(SystemCoreClock / 1000)` sets reload, clears counter,
enables interrupt, and selects HCLK as clock source in one call.

The ISR name is `SysTick_Handler` (defined in `src/systick.c`).