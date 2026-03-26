# VS Code + STM32 Bare-Metal Development Setup

## What Works and Why

This document records the working setup for editing, compiling, and navigating
STM32F446RE bare-metal code in VS Code. Written after a painful debugging session
so you never have to repeat it.

---

## The Stack

| Tool | Purpose |
|---|---|
| `arm-none-eabi-gcc` | Cross-compiler — builds the actual firmware |
| `bear` | Intercepts the build and generates `compile_commands.json` |
| `compile_commands.json` | Tells clangd exactly how each file is compiled |
| `clangd` (v16) | Language server — powers IntelliSense, Go-to-Definition, hover |
| Microsoft C/C++ extension | Keep installed — needed for the GDB debugger. IntelliSense is disabled |
| clangd VS Code extension | Active IntelliSense provider |

---

## Why compile_commands.json Is Required

In standard Linux C projects, clangd finds headers automatically from `/usr/include`.
This project is different — it has:

- **Non-standard include paths**: `CMSIS/core/` and `CMSIS/device/Include/`
- **Required define**: `-DSTM32F446xx` — without this, `stm32f4xx.h` does not
  include the right device header and symbols like `GPIOA` do not exist

`compile_commands.json` tells clangd about these. Without it, Go-to-Definition
breaks completely for all CMSIS types.

---

## Why bear Is Required

`bear` wraps the build (`bear -- make`) and records every compiler invocation into
`compile_commands.json`. This keeps IntelliSense in sync as you add source files.
Without bear, you would need to manually maintain `compile_commands.json`.

---

## Extensions — What to Keep, What to Remove

### Keep

| Extension | Why |
|---|---|
| **clangd** (LLVM) | Active IntelliSense provider |
| **Microsoft C/C++** | Debugger (GDB/OpenOCD). IntelliSense is disabled — keep it disabled |

### Remove / Disable for this workspace

| Extension | Why it breaks things |
|---|---|
| **C/C++ Runner** | Auto-generates `settings.json`, `launch.json`, and overwrites `c_cpp_properties.json` with native Linux gcc config on every reload |
| **Makefile Tools** | Intercepts IntelliSense and runs its own configuration, overriding `compile_commands.json` |

Both extensions reset `c_cpp_properties.json` to use `/usr/bin/gcc` (native Linux)
instead of `arm-none-eabi-gcc`, breaking navigation completely.

---

## Working .vscode Configuration

### settings.json
```json
{
  "C_Cpp.intelliSenseEngine": "disabled",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}",
    "--background-index",
    "--log=error"
  ]
}
```

`C_Cpp.intelliSenseEngine: disabled` prevents the Microsoft extension from
fighting with clangd. Without this, both try to provide IntelliSense and they
conflict.

### c_cpp_properties.json
```json
{
  "configurations": [
    {
      "name": "STM32F446RE",
      "compileCommands": "${workspaceFolder}/compile_commands.json",
      "compilerPath": "/usr/bin/arm-none-eabi-gcc",
      "intelliSenseMode": "linux-gcc-arm",
      "cStandard": "gnu11"
    }
  ],
  "version": 4
}
```

### tasks.json (build task)
The default build task (`Ctrl+Shift+B`) runs bear so `compile_commands.json`
stays up to date:

```json
{
  "label": "Build",
  "type": "shell",
  "command": "bear",
  "args": ["--", "make", "-j4"],
  "group": { "kind": "build", "isDefault": true }
}
```

---

## System Packages Required

```bash
sudo apt install bear clangd-16
sudo ln -sf /usr/bin/clangd-16 /usr/bin/clangd
```

Verify both work:
```bash
bear --version      # should print 3.x
clangd --version    # should print 16.x
```

---

## Daily Workflow

### Edit and compile in VS Code
- Write code in `src/`
- `Ctrl+Shift+B` — builds with `bear -- make -j4`, updates `compile_commands.json`
- Errors appear inline in the Problems panel

### Go-to-Definition
- **F12** — jump to definition
- **Alt+F12** — peek definition inline
- **Ctrl+click** — same as F12
- Works on all CMSIS types: `GPIOA`, `RCC`, `USART2`, struct fields like `MODER`, `ODR`

### If IntelliSense breaks after adding new files
Rebuild with `Ctrl+Shift+B` — bear regenerates `compile_commands.json` and
clangd re-indexes automatically.

### Flash firmware
```bash
make flash
# or from VS Code: Ctrl+Shift+P > Tasks: Run Task > Flash
```

---

## What Went Wrong During Setup (Lessons Learned)

1. **C/C++ Runner extension** kept silently overwriting `c_cpp_properties.json`
   with a native Linux gcc config. Every fix was undone on reload.

2. **Makefile Tools extension** intercepted IntelliSense and overrode
   `compile_commands.json` with its own dry-run parsing.

3. **`intelliSenseMode: "gcc-arm"`** is not a valid mode. Use `"linux-gcc-arm"`.

4. **`intelliSenseMode: "linux-gcc-arm64"`** is for native ARM64 Linux machines,
   not cross-compilation. It pointed IntelliSense at the wrong architecture headers.

5. **Relative include paths** in `compile_commands.json` (`-ICMSIS/core`) caused
   issues with some IntelliSense versions. Makefile now uses `$(abspath .)` to
   always produce absolute paths.

6. **clangd binary vs clangd extension** — the VS Code extension is just a wrapper.
   The actual `clangd` binary must be installed separately via `apt install clangd-16`.

7. **Microsoft C/C++ IntelliSense** does not work well with `arm-none-eabi-gcc`
   cross-compiler on Linux. Use clangd instead. Keep the extension only for GDB.

---

## Project Structure Reference

```
stm32-eink-dashboard/
├── CMSIS/
│   ├── core/               # 5 files — Cortex-M4 core headers
│   └── device/Include/     # 3 files — STM32F446 register definitions
├── src/
│   ├── main.c
│   ├── system_stm32f4xx.c
│   ├── syscalls.c
│   └── sysmem.c
├── startup/
│   └── startup_stm32f446xx.s
├── linker/
│   └── stm32f446re.ld
├── build/                  # generated — .elf, .hex, .bin, .map
├── compile_commands.json   # generated by bear — do not edit manually
├── Makefile
└── .vscode/
    ├── settings.json
    ├── c_cpp_properties.json
    ├── tasks.json
    └── launch.json
```
