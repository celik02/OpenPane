# OpenPane — Task List

> Bare-metal embedded display platform for STM32. Custom RTOS, multi-screen support, WiFi, MQTT, and OTA — built layer by layer from scratch.
>
> Track progress by checking boxes. Each phase must be fully validated before the next begins.

---

## Phase 1 — Custom RTOS Core

**Validation gate:** All 4 tasks running concurrently, UART printing task names and tick counts, both LEDs blinking at correct frequencies.

### Layer 1 — SysTick Timer

* [X] Configure SysTick reload value for 1ms tick (`SysTick->LOAD = (SystemCoreClock / 1000) - 1`)
* [ ] Enable SysTick with processor clock source
* [ ] Implement `SysTick_Handler` as `extern "C"` in C++
* [ ] Increment a global `volatile uint32_t g_tick` counter
* [X] **Validate:** GPIO toggles at 500Hz on scope/logic analyzer

### Layer 2 — Task Stack Initialization

* [X] Define `TaskControlBlock` struct (`sp` as first member, `func`, `state`, `name`, `stack_size`)
* [X] Implement `task_stack_init()` — fake the hardware exception entry stack frame
  * [X] Set xPSR = `0x01000000` (Thumb bit)
  * [X] Set PC = task function address
  * [X] Set LR = `0xFFFFFFFD` (EXC_RETURN, PSP, Thread mode)
  * [X] Zero out software-saved registers R4–R11 below the frame
* [X] Point `tcb->sp` to correct position after fake-stacking
* [ ] **Test infrastructure:** Add `tests/` directory with [doctest](https://github.com/doctest/doctest) single-header framework
* [ ] **Test infrastructure:** Add `make test` target to Makefile (native `clang++` build, no ARM toolchain)
* [ ] **Test:** Write `tests/test_rtos.cpp` — verify `init_tasks_stack()` fake frame layout
  * [ ] xPSR = `0x01000000` for all tasks
  * [ ] PC = correct task function address per task
  * [ ] LR = `0xFFFFFFFD` (EXC_RETURN, PSP, Thread mode)
  * [ ] R4–R11 slots below the frame are zero
* [ ] **Validate:** TCB stack pointer values look correct in GDB memory view (do NOT run scheduler yet)

### Layer 3 — PendSV Context Switch

* [ ] Implement `PendSV_Handler` in assembly
  * [ ] Save R4–R11 onto current task's PSP
  * [ ] Save PSP into `current_task->sp`
  * [ ] Call `scheduler_next_task()` to advance current task pointer
  * [ ] Load PSP from new task's `sp`
  * [ ] Load R4–R11 from new task's stack
  * [ ] `BX LR` to return
* [ ] Set PendSV to lowest priority: `NVIC_SetPriority(PendSV_IRQn, 0xFF)`
* [ ] Trigger PendSV from SysTick: `SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk`
* [ ] Implement first-launch bootstrap (switch MSP → PSP, jump to first task)
* [ ] **Validate:** Two tasks blinking LEDs at different rates, both confirmed over UART

### Layer 4 — Round-Robin Scheduler

* [ ] Define `task_list[MAX_TASKS]` and `current_task_idx`
* [ ] Implement `scheduler_next_task()` — circular index, skip BLOCKED tasks
* [ ] Implement `scheduler_start()` — init PSP, set first task, enable SysTick
* [ ] Protect task list access with `__disable_irq()` / `__enable_irq()` (or BASEPRI)
* [ ] **Validate:** All 4 tasks running, UART shows round-robin order, LEDs at correct frequencies

### Layer 5 — Mutex

* [ ] Implement `Mutex` class with `lock()` and `unlock()`
* [ ] Use LDREX/STREX for atomic test-and-set
* [ ] On lock failure: set task state to BLOCKED, trigger PendSV
* [ ] On unlock: unblock waiting task
* [ ] Protect mutex operations in a critical section
* [ ] **Validate:** Without mutex → garbled UART output. With mutex → clean interleaved output

### Phase 1 Debugging Checklist

* [ ] SysTick toggling GPIO at 500Hz (logic analyzer confirmed)
* [ ] TCB stack pointer values correct in GDB memory view
* [ ] EXC_RETURN value in PendSV LR is `0xFFFFFFFD`
* [ ] PendSV priority is `0xFF` (lowest)
* [ ] First task launch switches from MSP to PSP successfully
* [ ] Two tasks running concurrently (LED blink proof)
* [ ] UART output not garbled with mutex in place
* [ ] Dummy task does not starve other tasks (round-robin confirmed)

---

## Phase 2 — E-ink / Display via SPI-DMA

**Validation gate:** Image or text rendered on display from a dedicated RTOS task; scheduler continues running other tasks during transfer.

> **Before starting Phase 2:** Expand the test harness for all pure-logic code written from here onward — JSON parser, AT command parser, MQTT packet framing, CRC32. Hardware-specific layers (SPI, DMA, GPIO) do not need host tests; every data-processing layer does.

### Layer 1 — SPI Peripheral Init (Polling Mode)

* [ ] Enable SPI peripheral clock via RCC register
* [ ] Configure GPIO pins for AF mode (MOSI, SCK, CS) via direct register writes
* [ ] Configure SPI CR1: master mode, correct CPOL/CPHA, baud rate
* [ ] Implement `spi_transfer_byte()` with polling on TXE and BSY flags
* [ ] Send display init sequence in polling mode
* [ ] **Validate:** Logic analyzer confirms correct init sequence bytes on SPI pins

### Layer 2 — DMA Transfer

* [ ] Identify SPI TX DMA stream/channel from RM0390 DMA request mapping table
* [ ] Configure DMA stream: memory-to-peripheral, byte width, memory increment
* [ ] Set DMA source = framebuffer, destination = `&SPI->DR`, NDTR = buffer size
* [ ] Enable DMA Transfer Complete interrupt
* [ ] Implement `dma_transfer_complete_handler` — posts flag/semaphore for display task
* [ ] Disable then re-enable DMA stream for each new transfer
* [ ] **Validate:** Logic analyzer confirms DMA transfer; CPU free during transfer

### Layer 3 — Display Driver

* [ ] Identify display controller and obtain datasheet
* [ ] Implement `display_init()` — reset sequence, init commands, wait on BUSY pin
* [ ] Implement `display_write_framebuffer()` — kicks off DMA transfer
* [ ] Implement `display_refresh()` — sends refresh command
* [ ] Define framebuffer as 1-bit per pixel array sized for display resolution
* [ ] Implement `draw_pixel()` and `draw_text()` primitives
* [ ] **Validate:** Static image or text visible on display

### Layer 4 — RTOS Integration

* [ ] Implement `display_task` — waits on semaphore, copies pending framebuffer, triggers transfer
* [ ] Producer tasks write to pending framebuffer and post semaphore
* [ ] Protect framebuffer access with Phase 1 mutex
* [ ] **Validate:** UART and LED tasks still running at correct frequency during display refresh

### Phase 2 Debugging Checklist

* [ ] SPI clock polarity/phase matches display datasheet
* [ ] Logic analyzer confirms correct init sequence bytes
* [ ] DMA transfer complete interrupt fires correctly
* [ ] BUSY pin de-asserts after refresh
* [ ] Framebuffer contents visible on display
* [ ] No scheduler starvation during DMA transfer (UART timing unchanged)
* [ ] Double-buffer prevents tearing

---

## Phase 3 — WiFi Module Integration

**Validation gate:** STM32 connects to WiFi network and successfully pings an external IP or resolves DNS.

### Layer 1 — UART Communication with Module

* [ ] Configure second UART (UART1 or UART3) for ESP8266 at 115200 baud
* [ ] Implement interrupt-driven or DMA RX into a ring buffer
* [ ] Implement `uart_send_string()` using DMA TX
* [ ] Implement `uart_readline()` — reads until `\r\n` or timeout
* [ ] **Validate:** Send `AT\r\n`, receive `OK`

### Layer 2 — AT Command Parser

* [ ] Implement `at_send_command(cmd, expected_response, timeout_ms) -> bool`
* [ ] Implement response parser for `OK`, `ERROR`, `FAIL`, custom strings
* [ ] Implement multi-line response state machine
* [ ] Implement key AT commands: `AT`, `AT+RST`, `AT+CWMODE=1`, `AT+CWJAP`, `AT+CIFSR`, `AT+PING`
* [ ] **Validate:** IP address printed over UART2 to PC; device visible on router

### Layer 3 — WiFi Task

* [ ] Implement `wifi_task` — init module, connect to AP, maintain connection state
* [ ] Implement reconnect logic with exponential backoff
* [ ] Expose `wifi_is_connected()` status via mutex-protected shared state
* [ ] Store WiFi credentials in `secrets.h` (gitignored), not hardcoded
* [ ] **Validate:** WiFi task connects at boot; UART prints connection status; LED indicates connected

### Phase 3 Debugging Checklist

* [ ] `AT\r\n` → `OK` (basic UART communication confirmed)
* [ ] Module resets cleanly with `AT+RST`
* [ ] `AT+CWJAP` returns `OK` and `WIFI CONNECTED`
* [ ] `AT+CIFSR` returns a valid IP address
* [ ] Ping to `8.8.8.8` succeeds
* [ ] Connection survives 5 minutes without dropping
* [ ] Reconnect logic tested by power-cycling the router

---

## Phase 4 — Weather & Data Display

**Validation gate:** Current weather visible on display, auto-refreshing every N minutes.

### Layer 1 — HTTP GET (Plain HTTP)

* [ ] Implement TCP connection via AT commands (`AT+CIPSTART`)
* [ ] Implement `AT+CIPSEND` with correct HTTP GET request string
* [ ] Parse `+IPD,<len>:` prefix from ESP8266 response
* [ ] Store raw HTTP response in static buffer
* [ ] **Validate:** Raw HTTP response with JSON body printed to UART

### Layer 2 — JSON Parser

* [ ] Implement `find_json_string(json, key, out, max)`
* [ ] Implement `find_json_number(json, key, out)`
* [ ] Extract fields: `name`, `main.temp`, `main.humidity`, `weather[0].description`, `wind.speed`
* [ ] Use integer arithmetic (temp * 10) — avoid float
* [ ] **Validate:** Each parsed field printed over UART and matches website values

### Layer 3 — Display Layout

* [ ] Embed bitmap font as `const uint8_t[]` in flash
* [ ] Implement `render_weather()` — fills framebuffer from `WeatherData` struct
* [ ] Define layout positions for city, temp, description, humidity, wind, update time
* [ ] **Validate:** All fields visible on display, no overflow

### Layer 4 — Periodic Refresh

* [ ] Implement `task_delay()` for sleeping tasks (ticks-based)
* [ ] `http_task` sleeps N minutes between fetches
* [ ] Update shared `WeatherData` struct (mutex-protected), wake display task
* [ ] Only refresh display if data has changed
* [ ] **Validate:** Periodic refresh runs without stalling other tasks; stable after 30+ minutes

### Phase 4 Debugging Checklist

* [ ] Raw HTTP GET response visible in terminal
* [ ] JSON fields parse correctly (verify each field individually)
* [ ] Font renders correctly for all ASCII characters used
* [ ] Display layout fits within screen bounds
* [ ] Periodic refresh does not stall other tasks
* [ ] RTOS scheduler stable after 30+ minutes of operation

---

## Phase 5 — MQTT + Backend Pipeline

**Validation gate:** Sensor readings visible in real time on a dashboard on PC, with STM32 as data source.

### Layer 1 — Sensor Driver

* [ ] Choose sensor (DHT22, BME280, or internal ADC temp sensor)
* [ ] Configure ADC1 or SPI/I2C peripheral for chosen sensor
* [ ] Implement `sensor_read() -> SensorData`
* [ ] **Validate:** Raw sensor values printed over UART; verified against reference

### Layer 2 — MQTT Client via AT Commands

* [ ] Implement `mqtt_connect(broker_ip, port) -> bool` using `AT+MQTTUSERCFG` and `AT+MQTTCONN`
* [ ] Implement `mqtt_publish(topic, payload) -> bool` using `AT+MQTTPUB`
* [ ] Implement `mqtt_task` — reads sensor every N seconds, formats JSON payload, publishes
* [ ] **Validate:** `mosquitto_sub -h localhost -t sensors/#` on PC shows arriving messages

### Layer 3 — Desktop Backend

* [ ] Install and run Mosquitto broker locally
* [ ] Write Python subscriber using `paho-mqtt` to receive and print messages
* [ ] (Optional) Set up InfluxDB + Grafana or Node-RED for live dashboard
* [ ] **Validate:** Live sensor data visible on dashboard

### Layer 4 — Robustness

* [ ] Handle MQTT disconnect and reconnect
* [ ] Check `wifi_is_connected()` before publish; wait if not connected
* [ ] Implement watchdog — reset WiFi module if no publish in 60s
* [ ] **Validate:** Disconnect/reconnect cycle works without reboot; other tasks unaffected

### Phase 5 Debugging Checklist

* [ ] Sensor values look reasonable (compared to reference)
* [ ] MQTT broker running and accepting connections (`mosquitto -v`)
* [ ] `AT+MQTTCONN` returns `OK`
* [ ] Messages appear in `mosquitto_sub` on PC
* [ ] JSON payload parses correctly in Python
* [ ] Dashboard shows live data
* [ ] Disconnect/reconnect cycle works without reboot
* [ ] Other RTOS tasks unaffected during MQTT publish

---

## Phase 6 — OTA Firmware Update

**Validation gate:** Push new firmware to local HTTP server → device downloads, writes to flash, verifies CRC, reboots, runs new image. Old image recoverable on failure.

### Layer 1 — Flash Layout and Linker Scripts

* [ ] Map STM32F446 flash sectors from RM0390 Table 4 (sectors are non-uniform)
* [ ] Write `bootloader.ld`, `app_slot_a.ld`, `app_slot_b.ld` with correct `FLASH ORIGIN` and `LENGTH`
* [ ] Define `OtaMetadata` struct at fixed flash address (magic, addr, size, crc32, version)
* [ ] Place `FirmwareHeader` struct in dedicated `.fw_header` section via linker
* [ ] Write post-build Python script to patch `size` and `crc32` into binary after linking
* [ ] **Validate:** `arm-none-eabi-objdump -h` confirms correct load addresses, no slot overlap

### Layer 2 — Bootloader A/B Slot Awareness

* [ ] Read `OtaMetadata` at bootloader startup
* [ ] If magic valid: verify CRC32 of Slot B → erase Slot A → copy Slot B → Slot A → clear metadata → reboot
* [ ] If CRC fails: clear metadata, log error over UART, boot Slot A
* [ ] If no update pending: boot Slot A directly
* [ ] **Validate:** Manually flash known-good image to Slot B, trigger metadata via GDB, confirm bootloader applies update

### Layer 3 — HTTP Firmware Download in RTOS

* [ ] Implement `ota_task` — waits on semaphore from `mqtt_task`
* [ ] HTTP GET for `firmware_vN.bin` from local server
* [ ] Streaming flash write: buffer incoming bytes → write 32-bit words to Slot B
* [ ] After full download: write `OtaMetadata` to metadata sector
* [ ] Set graceful reboot flag (do not reboot mid-operation)
* [ ] **Validate:** Slot B flash content matches original binary byte-for-byte (verify in GDB)

### Layer 4 — MQTT OTA Trigger

* [ ] Subscribe `mqtt_task` to `ota/trigger` topic at init: `AT+MQTTSUB`
* [ ] Parse trigger payload: version, filename, size, expected CRC32
* [ ] Reject if `version <= current_version` (no downgrade)
* [ ] Post semaphore to wake `ota_task` with download parameters
* [ ] **Validate:** `mosquitto_pub` trigger on PC causes device to begin download

### Layer 5 — Watchdog and Rollback

* [ ] Enable IWDG during OTA — fires if download stalls >30s
* [ ] On reset after failed OTA: bootloader detects bad CRC, falls back to Slot A
* [ ] Implement post-update health check — new firmware must publish MQTT heartbeat within 60s
* [ ] Use RTC backup registers for OTA state that survives resets
* [ ] **Validate:** Power-cycle mid-download → device recovers and boots existing image

### Phase 6 Debugging Checklist

* [ ] Flash layout confirmed with objdump — no overlap between slots
* [ ] Bootloader reads OTA metadata and validates magic correctly
* [ ] CRC mismatch causes fallback to existing image
* [ ] Slot B flash content matches original binary
* [ ] Downgrade rejected (version check works)
* [ ] Watchdog fires if download stalls
* [ ] Device recovers from power loss mid-download
* [ ] Device recovers from power loss mid-flash-write
* [ ] New firmware sends MQTT heartbeat after successful update
* [ ] RTOS tasks still running during download

---

## Project Setup

* [X] Create GitHub repository: `openpane`
* [X] Add `LICENSE` (MIT)
* [X] Add `README.md` with project description and phase overview
* [ ] Add `secrets.h` to `.gitignore`
* [ ] Confirm Makefile builds cleanly on fresh clone
* [X] Add `TASKS.md` to repo root

---

*Generated from OpenPane Notion project plan. Notion is the source of truth for architecture decisions and learning resources — this file is for day-to-day progress tracking only.*
