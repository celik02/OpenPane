# STM32F446RE E-Ink Dashboard — Bare-Metal from Scratch

This project is a bare-metal STM32 application written **without HAL or CubeMX code generation**.
Every peripheral is configured by writing directly to hardware registers, using only CMSIS headers
for register definitions. The goal is to fully understand how the hardware works.

---

## Hardware

| Item         | Detail                   |
| ------------ | ------------------------ |
| Board        | NUCLEO-F446RE            |
| MCU          | STM32F446RET6            |
| Core         | ARM Cortex-M4 with FPU   |
| Flash        | 512 KB at `0x08000000` |
| SRAM         | 128 KB at `0x20000000` |
| System Clock | 84 MHz via PLL           |

### Onboard Peripherals Used

| Pin  | Label     | Function                 |
| ---- | --------- | ------------------------ |
| PA5  | LD2       | Green LED                |
| PC13 | B1        | Blue user button         |
| PA2  | USART2_TX | Serial TX → ST-Link USB |
| PA3  | USART2_RX | Serial RX → ST-Link USB |
| PA13 | SWDIO     | SWD debug                |
| PA14 | SWCLK     | SWD debug                |

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

## Prerequisites

Install the following tools before building:

| Tool                  | Purpose                                        | Install                                |
| --------------------- | ---------------------------------------------- | -------------------------------------- |
| `arm-none-eabi-gcc`   | ARM cross-compiler                             | `sudo apt install gcc-arm-none-eabi`   |
| `make`                | Build system                                   | `sudo apt install make`                |
| `openocd`             | On-chip debugger / flasher                     | `sudo apt install openocd`             |
| `bear`                | Generates `compile_commands.json` for clangd   | `sudo apt install bear`                |
| `gdb-multiarch`       | GDB with ARM support                           | `sudo apt install gdb-multiarch`       |

On macOS, replace `apt` with `brew`. The ARM toolchain package is `arm-none-eabi-gcc` on Homebrew as well.

For VS Code IntelliSense, this project uses **clangd**. Install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
and disable the default Microsoft C/C++ IntelliSense engine when prompted.

---

## Build System

The project uses a hand-written `Makefile`. No CMake, no IDE project files.

### Build the firmware

```bash
make
```

This produces three output files in `build/`:

| File                    | Description                                       |
| ----------------------- | ------------------------------------------------- |
| `build/dashboard.elf` | ELF with debug symbols — used by GDB and OpenOCD |
| `build/dashboard.hex` | Intel HEX — alternative flashing format          |
| `build/dashboard.bin` | Raw binary                                        |

If `bear` is installed, `make` automatically wraps the build with it and
regenerates `compile_commands.json` so clangd stays in sync.

### Other targets

```bash
make clean      # Remove the build/ directory
make flash      # Build and flash via OpenOCD (see Flashing section)
```

### Build flags

| Variable  | Default | Meaning                                         |
| --------- | ------- | ----------------------------------------------- |
| `DEBUG` | `1`   | Adds `-g -gdwarf-2`; set to `0` for release |
| `OPT`   | `-Og` | Optimise for debugging; use `-O2` for release |

Override on the command line:

```bash
make DEBUG=0 OPT=-O2
```

---

## Flashing & Debugging

The NUCLEO-F446RE has an onboard **ST-Link v2-1** debugger connected over USB.
No external programmer is needed. OpenOCD talks to it over SWD (PA13 SWDIO / PA14 SWCLK).

### Flash with make

```bash
make flash
```

This runs:

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
        -c "program build/dashboard.elf verify reset exit"
```

OpenOCD programs the ELF, verifies the write, then resets the MCU and exits.

### Flash manually with OpenOCD

```bash
# Start OpenOCD server (keep this terminal open)
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# In a second terminal, connect GDB
gdb-multiarch build/dashboard.elf
(gdb) target extended-remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) monitor reset run
```

### Debug in VS Code

1. Install the [Cortex-Debug extension](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug).
2. Open the Run & Debug panel (`Ctrl+Shift+D`).
3. Select **Debug (OpenOCD)** and press `F5`.

The launch config (`.vscode/launch.json`) will:

- Run the **Build** task automatically before launching
- Connect to the ST-Link via OpenOCD
- Halt at the entry point of `main()`
- Load the SVD file so peripheral registers are visible in the **Cortex Peripherals** panel

### Debug with GDB on the command line

```bash
# Terminal 1 — start OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# Terminal 2 — attach GDB
gdb-multiarch build/dashboard.elf
(gdb) target extended-remote :3333
(gdb) monitor reset halt   # halt the MCU
(gdb) load                 # flash the binary
(gdb) break main           # set a breakpoint
(gdb) continue             # run to breakpoint
```

### Serial output (USART2)

The ST-Link exposes USART2 as a USB virtual COM port. Connect with any serial terminal at **115200 8N1**:

```bash
# Linux — device is usually /dev/ttyACM0
screen /dev/ttyACM0 115200

# or
minicom -D /dev/ttyACM0 -b 115200
```

On macOS the device will appear as `/dev/tty.usbmodem*`.

---

## Learning Resources

- [STM32F446RE Reference Manual (RM0390)](https://www.st.com/resource/en/reference_manual/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) — the bible. Every register explained.
- [STM32F446RE Datasheet](https://www.st.com/resource/en/datasheet/stm32f446re.pdf) — pin functions, electrical specs
- [ARM Cortex-M4 Technical Reference Manual](https://developer.arm.com/documentation/100166/0001/) — core internals
- [NUCLEO-F446RE User Manual (UM1724)](https://www.st.com/resource/en/user_manual/um1724-stm32-nucleo64-boards-mb1136-stmicroelectronics.pdf) — board schematic, ST-Link
