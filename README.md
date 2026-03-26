# STM32F446RE E-Ink Dashboard — Bare-Metal from Scratch

This project is a bare-metal STM32 application written **without HAL or CubeMX code generation**.
Every peripheral is configured by writing directly to hardware registers, using only CMSIS headers
for register definitions. The goal is to fully understand how the hardware works.

---

## Hardware

| Item | Detail |
|------|--------|
| Board | NUCLEO-F446RE |
| MCU | STM32F446RET6 |
| Core | ARM Cortex-M4 with FPU |
| Flash | 512 KB at `0x08000000` |
| SRAM | 128 KB at `0x20000000` |
| System Clock | 84 MHz via PLL |

### Onboard Peripherals Used

| Pin | Label | Function |
|-----|-------|----------|
| PA5 | LD2 | Green LED |
| PC13 | B1 | Blue user button |
| PA2 | USART2_TX | Serial TX → ST-Link USB |
| PA3 | USART2_RX | Serial RX → ST-Link USB |
| PA13 | SWDIO | SWD debug |
| PA14 | SWCLK | SWD debug |

---

## Why No HAL?

ST provides a Hardware Abstraction Layer (HAL) and CubeMX to auto-generate initialization code.
HAL is useful for production projects but hides all the details. In this project we configure
every peripheral by reading the reference manual and writing to registers directly. This means:

- You understand *why* each bit is set
- You can debug hardware problems without guessing what HAL is doing underneath
- You learn skills that transfer to any microcontroller, not just STM32

---

## Project Structure

```
stm32-eink-dashboard/
├── CMSIS/
│   ├── core/               # ARM Cortex-M4 core headers (from ARM-software/CMSIS_5)
│   └── device/             # STM32F4 peripheral register definitions (from STMicroelectronics/cmsis_device_f4)
├── src/
│   └── main.c              # Application code
├── startup/
│   └── startup_stm32f446xx.s  # Reset handler and vector table (assembly)
├── linker/
│   └── stm32f446re.ld      # Linker script — memory map and section placement
├── .vscode/
│   ├── c_cpp_properties.json  # IntelliSense config
│   ├── tasks.json             # Build task (Ctrl+Shift+B)
│   └── launch.json            # Debug config (Cortex-Debug + OpenOCD)
└── Makefile                # Build system
```

---

## CMSIS Headers

CMSIS (Cortex Microcontroller Software Interface Standard) is a vendor-neutral API defined by ARM.
It provides two things we use:

### 1. `CMSIS/core/` — from ARM
Source: https://github.com/ARM-software/CMSIS_5

Describes the **Cortex-M4 CPU core** itself. Chip-independent — same files work for
any Cortex-M4 regardless of manufacturer.

Key files:
- `core_cm4.h` — NVIC (interrupt controller), SysTick, MPU, FPU register access
- `cmsis_gcc.h` — GCC-specific intrinsics (`__NOP()`, `__WFI()`, `__disable_irq()`, etc.)
- `cmsis_compiler.h` — selects the right compiler-specific header automatically

How to reinstall:
```bash
mkdir -p CMSIS/core
git clone --depth=1 --filter=blob:none --sparse https://github.com/ARM-software/CMSIS_5 CMSIS/core_tmp
git -C CMSIS/core_tmp sparse-checkout set CMSIS/Core/Include
cp CMSIS/core_tmp/CMSIS/Core/Include/*.h CMSIS/core/
rm -rf CMSIS/core_tmp
```

### 2. `CMSIS/device/` — from ST
Source: https://github.com/STMicroelectronics/cmsis_device_f4

Describes every **STM32F4 peripheral** as C structs mapped to their hardware addresses.
This is what allows you to write readable code like:

```c
GPIOA->MODER |= (1 << 10);   // Set PA5 as output
GPIOA->ODR   |= (1 << 5);    // Set PA5 high
```

Instead of raw pointer arithmetic:
```c
*(volatile uint32_t*)0x40020000 |= (1 << 10);
*(volatile uint32_t*)0x40020014 |= (1 << 5);
```

Key files:
- `Include/stm32f446xx.h` — every register of every peripheral (GPIO, RCC, USART, SPI, etc.)
- `Include/stm32f4xx.h` — top-level include; selects the right chip header via `#define STM32F446xx`
- `Include/system_stm32f4xx.h` — declares `SystemInit()` and `SystemCoreClock`
- `Source/Templates/system_stm32f4xx.c` — default `SystemInit()` implementation (we replace this)

How to reinstall:
```bash
git clone --depth=1 https://github.com/STMicroelectronics/cmsis_device_f4 CMSIS/device
```

---

## Clock Configuration

The STM32F446RE has several clock sources. We use the PLL to reach 84 MHz:

```
HSI (16 MHz internal RC)
    │
    └──> PLL ──> SYSCLK = 84 MHz ──> AHB = 84 MHz ──> APB1 = 42 MHz
                                                    └──> APB2 = 84 MHz
PLL formula:
    VCO input  = HSI / PLLM  = 16 / 16 = 1 MHz
    VCO output = VCO * PLLN  = 1 * 336 = 336 MHz
    SYSCLK     = VCO / PLLP  = 336 / 4 = 84 MHz
```

Flash latency must be set to 2 wait states at 84 MHz (required per datasheet).

---

## Build System

> *(Makefile and build instructions will be added here)*

## Flashing & Debugging

> *(OpenOCD + GDB setup will be added here)*

---

## Learning Resources

- [STM32F446RE Reference Manual (RM0390)](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) — the bible. Every register explained.
- [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) — pin functions, electrical specs
- [ARM Cortex-M4 Technical Reference Manual](https://developer.arm.com/documentation/100166/0001/) — core internals
- [NUCLEO-F446RE User Manual (UM1724)](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf) — board schematic, ST-Link
