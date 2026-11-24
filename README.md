# MCP23017 STM32 C Driver

A lightweight, multi-instance C driver for the **Microchip MCP23017** 16-bit I2C I/O Expander, built for **STM32 HAL** (`stm32f1xx_hal.h`) with a decoupled I2C bus abstraction layer (`abs_i2c.h`).

It provides clean, zero-allocation control over both 8-bit GPIO ports (`PORTA` and `PORTB`), internal 100 kΩ pull-up resistors, atomic 16-bit burst reads, interrupt-on-change configuration, and `INTA`/`INTB` interrupt mirroring for bare-metal and RTOS (e.g., FreeRTOS) firmware.

---

## Repository Contents

| File | Description |
| :--- | :--- |
| `udrv_mcp23017.h` | Public header: MCP23017 register map (`BANK = 0`), bitmask macros, port/interrupt enums, `MCP23017_States` device handle struct, and API prototypes. |
| `udrv_mcp23017.c` | Driver implementation: device registry, port I/O, burst reads, pull-up configuration, interrupt-on-change setup, `IOCON.MIRROR` control, and ISR flag helper. |
| `README.md` | Driver architecture, configuration, API reference, and integration guide. |

---

## Key Features

- **Multi-Device Support on Shared or Separate I2C Buses**: Each MCP23017 chip is encapsulated in its own `MCP23017_States` handle and registered in an internal device table (`max_mcp_devices = 4`, configurable up to 8 per I2C bus).
- **Full 16-Bit Port Control**: Configure pin directions (`IODIRA`/`IODIRB`), enable internal 100 kΩ pull-ups (`GPPUA`/`GPPUB`), write outputs, and read inputs per 8-bit port with arbitrary bitmasks.
- **Single-Transaction Dual-Port Reads**: `udrv_mcp23017_read_both_ports()` reads `GPIOA` and `GPIOB` in a single 2-byte I2C burst transaction using MCP23017 sequential addressing.
- **Event-Driven Interrupt-on-Change**: Configures `INTCON` and `GPINTEN` so any input transition triggers `INTA`/`INTB`, avoiding continuous I2C polling.
- **Interrupt Mirroring (`IOCON.MIRROR`)**: Internally ORs `INTA` and `INTB` using a safe read-modify-write on `IOCONA`, allowing a **single STM32 EXTI pin** to monitor all 16 expanded pins on a chip.
- **Decoupled I2C Transport Layer**: Uses `abs_i2c.h` (`i2c_write_reg` / `i2c_read_reg`) so bus locking (e.g., FreeRTOS mutexes), DMA/IT transfers, or unit-test mocks can be added without modifying driver code.
- **Consistent Error Reporting**: All I2C-backed functions propagate STM32 `HAL_StatusTypeDef` (`HAL_OK`, `HAL_ERROR`, `HAL_BUSY`, `HAL_TIMEOUT`).
- **Static, Zero-Heap Footprint**: No `malloc`/`free`, minimal RAM usage per device instance, and deterministic execution suitable for embedded targets.

---

## Why & How This Driver Works Well

1. **CMSIS-Safe Register Naming**  
   STM32 device headers define `GPIOA` and `GPIOB` as peripheral base-address pointers. This driver deliberately names the MCP23017 port registers `GPIOA_REG` (`0x12`) and `GPIOB_REG` (`0x13`) to prevent preprocessor collisions while keeping register definitions readable.

2. **Halved I2C Overhead with Sequential Burst Reads**  
   In MCP23017's power-on default `BANK = 0` mode, `GPIOA` (`0x12`) and `GPIOB` (`0x13`) occupy adjacent register addresses and the internal address pointer auto-increments (`SEQOP = 0`). `udrv_mcp23017_read_both_ports()` leverages this to read all 16 bits in one 2-byte I2C transfer (`data[0]` = Port A, `data[1]` = Port B), reducing bus time and clearing interrupts on both ports simultaneously.

3. **Non-Destructive `IOCON` Modification**  
   `udrv_mcp23017_mirror_int()` performs a read-modify-write on `IOCONA` (`0x0A`), toggling only bit 6 (`0x40`, `MIRROR`). Other `IOCON` settings (`BANK`, `SEQOP`, `DISSLW`, `HAEN`, `ODR`, `INTPOL`) remain untouched.

4. **Reduced MCU Pin Count via Mirrored Interrupts**  
   Enabling `MIRROR` routes interrupts from both `PORTA` and `PORTB` to both `INTA` and `INTB`. You only need **1 STM32 EXTI pin per MCP23017** (for example, 3 EXTI pins to monitor 48 digital inputs across 3 expanders).

5. **Clean Separation Between Hardware Bus and Chip Logic**  
   Routing I2C transfers through `abs_i2c.h` keeps `udrv_mcp23017.c` independent of raw HAL calls. If multiple RTOS tasks share one I2C peripheral, mutex protection can be added once inside `i2c_write_reg()` / `i2c_read_reg()` rather than scattered across driver calls.

6. **Short-Circuit Multi-Register Writes**  
   Compound configuration routines (`udrv_mcp23017_setting_int()`, `udrv_mcp23017_mirror_int()`) check `HAL_OK` after the first I2C transaction before attempting the second, failing fast on NACKs or bus timeouts.

---

## Hardware & Register Map (`BANK = 0`)

The driver targets MCP23017's default `IOCON.BANK = 0` register layout:

| Register Macro | Address | Function |
| :--- | :---: | :--- |
| `IODIRA` / `IODIRB` | `0x00` / `0x01` | I/O Direction (`1` = Input, `0` = Output) |
| `GPINTENA` / `GPINTENB` | `0x04` / `0x05` | Interrupt-on-Change Enable (`1` = Enabled, `0` = Disabled) |
| `DEFVALA` / `DEFVALB` | `0x06` / `0x07` | Default Compare Register for Interrupt-on-Change |
| `INTCONA` / `INTCONB` | `0x08` / `0x09` | Interrupt Control (`0` = Compare to previous pin value, `1` = Compare to `DEFVAL`) |
| `IOCONA` / `IOCONB` | `0x0A` / `0x0B` | Expander Configuration (`Bit 6` = `MIRROR`) |
| `GPPUA` / `GPPUB` | `0x0C` / `0x0D` | Internal 100 kΩ Pull-Up Enable (`1` = Enabled, `0` = Disabled) |
| `GPIOA_REG` / `GPIOB_REG` | `0x12` / `0x13` | General-Purpose I/O Port Register (Read/Write) |

### Convenience Bitmask Macros

| Macro | Value | Meaning |
| :--- | :---: | :--- |
| `IODIRA_IN`, `IODIRB_IN`, `H_Sig` | `0xFF` | All 8 pins high / configured as inputs |
| `IODIRA_OUT`, `IODIRB_OUT`, `L_Sig` | `0x00` | All 8 pins low / configured as outputs |
| `GPINTENA_HIGH`, `GPINTENB_HIGH` | `0xFF` | Enable interrupt-on-change on all 8 pins |
| `GPINTENA_LOW`, `GPINTENB_LOW` | `0x00` | Disable interrupt-on-change on all 8 pins |
| `GPPUA_HIGH`, `GPPUB_HIGH` | `0xFF` | Enable internal pull-ups on all 8 pins |
| `ALL_GPIOA_HIGH`, `ALL_GPIOB_HIGH` | `0xFF` | Drive all 8 port pins high |
| `ALL_GPIOA_LOW`, `ALL_GPIOB_LOW` | `0x00` | Drive all 8 port pins low |

### I2C Addressing (8-Bit Shifted Format)

STM32 HAL `HAL_I2C_Mem_*` functions expect the **8-bit left-shifted address** (`7-bit address << 1`). Pass the shifted address directly to `udrv_mcp23017_init()`:

| `A2` | `A1` | `A0` | 7-Bit Address | 8-Bit Address (Pass to `init`) |
| :--: | :--: | :--: | :-----------: | :----------------------------: |
| 0 | 0 | 0 | `0x20` | **`0x40`** |
| 0 | 0 | 1 | `0x21` | **`0x42`** |
| 0 | 1 | 0 | `0x22` | **`0x44`** |
| 0 | 1 | 1 | `0x23` | **`0x46`** |
| 1 | 0 | 0 | `0x24` | **`0x48`** |
| 1 | 0 | 1 | `0x25` | **`0x4A`** |
| 1 | 1 | 0 | `0x26` | **`0x4C`** |
| 1 | 1 | 1 | `0x27` | **`0x4E`** |

---

## Configuration & Prerequisites

### 1. STM32 HAL Header
`udrv_mcp23017.h` includes `stm32f1xx_hal.h`. To use this driver on another STM32 family (F4, G4, L4, H7, etc.), replace `#include "stm32f1xx_hal.h"` with your target series HAL header (e.g., `stm32f4xx_hal.h`).

### 2. I2C Abstraction Layer (`abs_i2c.h`)
`udrv_mcp23017.c` calls two bus functions declared in `abs_i2c.h`. Add this minimal shim to your project (or wrap it with a FreeRTOS mutex if multiple tasks share the I2C bus):

**`abs_i2c.h`**
```c
#ifndef INC_ABS_I2C_H_
#define INC_ABS_I2C_H_

#include "stm32f1xx_hal.h"

HAL_StatusTypeDef i2c_write_reg(I2C_HandleTypeDef *hi2c, uint8_t dev_address,
                                uint8_t reg_address, uint8_t MemAddSize,
                                uint8_t *value, uint8_t Size, uint8_t Timeout);

HAL_StatusTypeDef i2c_read_reg(I2C_HandleTypeDef *hi2c, uint8_t dev_address,
                               uint8_t reg_address, uint8_t MemAddSize,
                               uint8_t *value, uint8_t Size, uint8_t Timeout);

#endif
```

**`abs_i2c.c`**
```c
#include "abs_i2c.h"

HAL_StatusTypeDef i2c_write_reg(I2C_HandleTypeDef *hi2c, uint8_t dev_address,
                                uint8_t reg_address, uint8_t MemAddSize,
                                uint8_t *value, uint8_t Size, uint8_t Timeout)
{
    return HAL_I2C_Mem_Write(hi2c, dev_address, reg_address, MemAddSize, value, Size, Timeout);
}

HAL_StatusTypeDef i2c_read_reg(I2C_HandleTypeDef *hi2c, uint8_t dev_address,
                               uint8_t reg_address, uint8_t MemAddSize,
                               uint8_t *value, uint8_t Size, uint8_t Timeout)
{
    return HAL_I2C_Mem_Read(hi2c, dev_address, reg_address, MemAddSize, value, Size, Timeout);
}
```

### 3. Maximum Registered Devices
In `udrv_mcp23017.c`, the internal registry size defaults to 4:
```c
#define max_mcp_devices 4
```
Increase this value (up to `8` per I2C bus, or more across multiple I2C peripherals) if your board uses more than 4 MCP23017 devices.

### 4. STM32 CubeMX Peripheral Setup
- **I2C**: Configure your I2C peripheral (e.g., `I2C1` or `I2C2`) in standard mode (100 kHz) or fast mode (400 kHz).
- **EXTI (if using interrupts)**: Configure the MCU GPIO connected to MCP23017 `INTA`/`INTB` as External Interrupt Mode with **Falling Edge** trigger (MCP23017 `INT` outputs default to active-low push-pull, `IOCON.INTPOL = 0`, `IOCON.ODR = 0`).

---

## Data Types

```c
typedef enum {
    MCP_PORT_A = 0,
    MCP_PORT_B = 1,
} MCP_Port;

typedef enum {
    INT_OFF = 0,
    INT_ON  = 1
} MCP_INT;

typedef struct {
    I2C_HandleTypeDef *hi2c;    // Pointer to STM32 HAL I2C handle
    uint8_t i2c_address;        // 8-bit shifted I2C address (0x40 - 0x4E)
    uint16_t exti_pin;          // STM32 EXTI GPIO pin mapped to INTA/INTB
    MCP_Port int_port;          // Last configured interrupt port
    volatile uint8_t int_flag;  // ISR-safe pending interrupt flag
} MCP23017_States;
```

---

## API Reference

### `udrv_mcp23017_init`
```c
void udrv_mcp23017_init(MCP23017_States *dev, I2C_HandleTypeDef *hi2c, uint8_t dev_address);
```
Initializes a device handle, assigns its I2C bus and 8-bit address, clears `int_flag` and `exti_pin`, and registers the handle in the internal `mcp_registry` (up to `max_mcp_devices`). Does not perform I2C traffic itself.

---

### `udrv_mcp23017_set_port_dir`
```c
HAL_StatusTypeDef udrv_mcp23017_set_port_dir(MCP23017_States *dev, MCP_Port port, uint8_t dir_mask);
```
Configures pin directions for `MCP_PORT_A` (`IODIRA`) or `MCP_PORT_B` (`IODIRB`).
- Bit `1` = Input, Bit `0` = Output.
- Supports whole-port macros (`IODIRA_IN`, `IODIRA_OUT`, `H_Sig`, `L_Sig`) or mixed masks (e.g., `0x0F` sets pins 0–3 as inputs and 4–7 as outputs).

---

### `udrv_mcp23017_pullup`
```c
HAL_StatusTypeDef udrv_mcp23017_pullup(MCP23017_States *dev, MCP_Port port, uint8_t pullup_mask);
```
Enables or disables the internal 100 kΩ pull-up resistors on `GPPUA` or `GPPUB`.
- Bit `1` = Pull-up enabled (applies to pins configured as inputs), Bit `0` = Pull-up disabled.

---

### `udrv_mcp23017_write_port`
```c
HAL_StatusTypeDef udrv_mcp23017_write_port(MCP23017_States *dev, MCP_Port port, uint8_t value);
```
Writes an 8-bit output state directly to `GPIOA_REG` (`0x12`) or `GPIOB_REG` (`0x13`).

---

### `udrv_mcp23017_read_port`
```c
HAL_StatusTypeDef udrv_mcp23017_read_port(MCP23017_States *dev, MCP_Port port, uint8_t *data);
```
Reads the current 8-bit logic state of `GPIOA_REG` or `GPIOB_REG` into `*data`. Reading the port register also clears any latched interrupt on that port.

---

### `udrv_mcp23017_read_both_ports`
```c
HAL_StatusTypeDef udrv_mcp23017_read_both_ports(MCP23017_States *dev, uint8_t *data);
```
Reads both `GPIOA` and `GPIOB` in a single 2-byte sequential I2C read starting at `GPIOA_REG` (`0x12`).
- `data` must point to a buffer of at least **2 bytes**: `data[0]` receives `PORTA`, and `data[1]` receives `PORTB`.
- Clears pending interrupts on both ports simultaneously.

---

### `udrv_mcp23017_setting_int`
```c
HAL_StatusTypeDef udrv_mcp23017_setting_int(MCP23017_States *dev, MCP_Port port, MCP_INT intpt_val, uint16_t exti_pin);
```
Configures interrupt-on-change for an entire 8-bit port:
1. Saves `port` and `exti_pin` into `dev`.
2. Writes `0x00` to `INTCONA` or `INTCONB` so pins trigger interrupts on **any state change relative to their previous value**.
3. Writes `0xFF` (`INT_ON`) or `0x00` (`INT_OFF`) to `GPINTENA` or `GPINTENB`.

---

### `udrv_mcp23017_mirror_int`
```c
HAL_StatusTypeDef udrv_mcp23017_mirror_int(MCP23017_States *dev, MCP_INT mirror_state);
```
Reads `IOCONA` (`0x0A`), sets (`INT_ON`) or clears (`INT_OFF`) bit 6 (`0x40`, `MIRROR`), and writes the updated value back. When enabled, `INTA` and `INTB` are internally ORed together so any interrupt on either port asserts both pins.

---

### `udrv_mcp23017_checking_int`
```c
uint8_t udrv_mcp23017_checking_int(MCP23017_States *dev);
```
Checks whether `dev->int_flag` was raised by an EXTI interrupt handler. If raised (`1`), resets `dev->int_flag` to `0` and returns `1`; otherwise returns `0`.

---

## Usage Examples

### 1. Multi-Device Input Expander Setup with Mirrored Interrupts

```c
#include "udrv_mcp23017.h"

extern I2C_HandleTypeDef hi2c2;

MCP23017_States mcp1;
MCP23017_States mcp2;

void app_mcp_init(void)
{
    // Device 1 at 8-bit address 0x40 (A2=0, A1=0, A0=0) -> EXTI on GPIO_PIN_13
    udrv_mcp23017_init(&mcp1, &hi2c2, 0x40);
    udrv_mcp23017_set_port_dir(&mcp1, MCP_PORT_A, H_Sig);   // All inputs
    udrv_mcp23017_set_port_dir(&mcp1, MCP_PORT_B, H_Sig);   // All inputs
    udrv_mcp23017_pullup(&mcp1, MCP_PORT_A, H_Sig);         // 100k pull-ups on
    udrv_mcp23017_pullup(&mcp1, MCP_PORT_B, H_Sig);         // 100k pull-ups on
    udrv_mcp23017_setting_int(&mcp1, MCP_PORT_A, INT_ON, GPIO_PIN_13);
    udrv_mcp23017_setting_int(&mcp1, MCP_PORT_B, INT_ON, GPIO_PIN_13);
    udrv_mcp23017_mirror_int(&mcp1, INT_ON);                // Mirror INTA & INTB

    // Device 2 at 8-bit address 0x42 (A2=0, A1=0, A0=1) -> EXTI on GPIO_PIN_15
    udrv_mcp23017_init(&mcp2, &hi2c2, 0x42);
    udrv_mcp23017_set_port_dir(&mcp2, MCP_PORT_A, H_Sig);
    udrv_mcp23017_set_port_dir(&mcp2, MCP_PORT_B, H_Sig);
    udrv_mcp23017_pullup(&mcp2, MCP_PORT_A, H_Sig);
    udrv_mcp23017_pullup(&mcp2, MCP_PORT_B, H_Sig);
    udrv_mcp23017_setting_int(&mcp2, MCP_PORT_A, INT_ON, GPIO_PIN_15);
    udrv_mcp23017_setting_int(&mcp2, MCP_PORT_B, INT_ON, GPIO_PIN_15);
    udrv_mcp23017_mirror_int(&mcp2, INT_ON);
}
```

### 2. Reading Both Ports on Interrupt (FreeRTOS or Bare-Metal)

**FreeRTOS Task-Notification Pattern:**
```c
// In HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin):
if (GPIO_Pin == GPIO_PIN_13 && DIO_TaskHandle != NULL) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(DIO_TaskHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Inside your DIO task:
uint8_t ports[2] = {0};
for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (udrv_mcp23017_read_both_ports(&mcp1, ports) == HAL_OK) {
        uint8_t port_a = ports[0];
        uint8_t port_b = ports[1];
        // Process 16-bit input state...
    }
}
```

**Bare-Metal Flag Pattern (using `int_flag` & `udrv_mcp23017_checking_int`):**
```c
// Uncomment ucbk_stm32_callback() in udrv_mcp23017.c and call it from HAL_GPIO_EXTI_Callback():
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    ucbk_stm32_callback(GPIO_Pin);
}

// In main loop:
uint8_t ports[2] = {0};
if (udrv_mcp23017_checking_int(&mcp1)) {
    udrv_mcp23017_read_both_ports(&mcp1, ports);
}
```

### 3. Driving Outputs

```c
MCP23017_States mcp_out;

udrv_mcp23017_init(&mcp_out, &hi2c1, 0x44);
udrv_mcp23017_set_port_dir(&mcp_out, MCP_PORT_A, IODIRA_OUT); // All outputs
udrv_mcp23017_write_port(&mcp_out, MCP_PORT_A, 0xAA);         // Alternating high/low
```

---

## Integration Notes & Future Roadmap

- **Always Clear Latched Interrupts**: After an MCP23017 interrupt fires (or right after enabling interrupts during boot), read `GPIOA`/`GPIOB` (e.g., via `udrv_mcp23017_read_both_ports()`) so the hardware releases the `INTA`/`INTB` line for future edges.
- **Per-Pin Helpers**: Port-wide bitmasks already allow mixed pin directions and pull-ups. Dedicated pin-wise configuration helpers (`/* TODO: Pin wise config */` in `udrv_mcp23017.h`) as well as per-pin `GPINTEN`/`DEFVAL`/`INTF`/`INTCAP` accessors can be layered cleanly on top of the existing API.
- **I2C Timeout**: All register transactions use a `100` ms HAL timeout by default.
