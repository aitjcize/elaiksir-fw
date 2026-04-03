# Elaiksir Firmware Monorepo

ESP-IDF firmware for the elaiksir beverage machine product line.

## Repository Structure

```
elaiksir-fw/
├── shared/components/       # Reusable across all products
│   ├── motor_if/            # Abstract motor interface (vtable ops)
│   ├── temp_sensor_if/      # Abstract temperature sensor interface
│   ├── heater_if/           # Abstract heater interface
│   ├── weight_sensor_if/    # Abstract weight/load cell interface
│   ├── drv_tca9548/         # TCA9548A I2C 8-ch mux
│   ├── drv_pca9685/         # PCA9685 16-ch 12-bit PWM
│   ├── drv_ads1115/         # ADS1115 16-bit 4-ch ADC
│   ├── drv_nau7802/         # NAU7802 24-bit load cell ADC
│   ├── drv_max31865/        # MAX31865 PT100/PT1000 RTD (SPI)
│   └── drv_tca9555/         # TCA9555 16-bit GPIO expander
├── matcha-bowl/             # Product: ESP32-WROOM-32E matcha machine
├── liquid-machine/          # Product: ESP32-P4 liquid dispensing machine
└── CLAUDE.md
```

Each product directory is a standalone ESP-IDF project. Build by `cd`-ing into the product directory and running `idf.py build`. Shared components are referenced via `EXTRA_COMPONENT_DIRS = "../shared/components"` in each product's `CMakeLists.txt`.

## Build

```bash
export IDF_PATH=~/Work/esp-idf    # ESP-IDF v6.0-beta1
source $IDF_PATH/export.sh

# Matcha bowl
cd matcha-bowl && idf.py set-target esp32 && idf.py build

# Liquid machine
cd liquid-machine && idf.py set-target esp32p4 && idf.py build
```

## Products

### matcha-bowl (ESP32-WROOM-32E)
AC-powered matcha mixing machine. Schematic: `matcha-bowl/schematic/matcha_machine_0227.pdf`.
- Dual heater: relay (GPIO26) + TRIAC phase-angle (GPIO19) with zero-crossing ISR (GPIO36)
- Motor: LEDC PWM (GPIO23) + current sense ADC (GPIO34)
- NTC temperature: ADC (GPIO39), 10K B3950 thermistor
- OLED display: SPI (GPIO5/17/16/4) — controller TBD, driver is a stub
- Touch: 4x TTP223 (GPIO35/32/33/25)
- RGB LED: power MOSFET (GPIO27) + PWM (GPIO14)
- State machine: IDLE → HEATING → MIXING → DONE
- CLI console over UART: `heater`, `motor`, `temp`, `touch`, `rgb`, `display`, `status`

### liquid-machine (ESP32-P4)
24V DC liquid dispensing machine. Schematic: `liquid-machine/schematic/MM32P4_V1_Schematic_0402.pdf`.
- 3x I2C buses with ISO1540 isolators and TCA9548 muxes
- 10x DC motors: PCA9685 PWM → DRV8841 H-bridge (24V)
- 6x solenoid valves via DRV8841
- 4x NAU7802 load cells (reservoir inventory monitoring)
- 2x MAX31865 PT100 temperature sensors (SPI)
- Ethernet: LAN8720A RMII
- Flow sensor: PCNT pulse counting
- HTTP API: pump control, time-based order execution, reservoir levels, settings (NVS)
- Dispensing is time-based (amount_ml / ml_per_second). Load cells are for inventory only.
- CLI console over UART: `pump`, `stop-all`, `valve`, `scale`, `temp`, `flow`, `net`, `status`

## Architecture Patterns

### Abstract interfaces (vtable ops)
Shared interfaces use a C vtable pattern. Each product provides backend implementations:

```c
// Shared interface (e.g. motor_if.h)
struct motor { const char *name; const motor_ops_t *ops; void *ctx; };

// Product backend (e.g. matcha-bowl motor.h)
motor_t *motor_matcha_create(void);       // LEDC PWM backend

// Product backend (e.g. liquid-machine motor_drv8841.h)
motor_t *motor_drv8841_create(name, cfg); // I2C→PCA9685→DRV8841 backend

// Application code uses the abstract interface
motor_set_speed(motor, 70);  // works with any backend
```

When adding new peripheral abstractions, follow this pattern: define ops struct in `shared/components/*_if/`, implement backend in product `components/`, export a `*_create()` factory function.

### IC drivers (shared/components/drv_*)
Generic I2C/SPI device drivers. Stateless where possible — caller manages device instance structs. All use ESP-IDF v6.0 `i2c_master` API (not legacy i2c driver).

### Product-specific components
Components in `<product>/components/` contain hardware-specific code that implements shared interfaces or provides product-only functionality (e.g. heater, touch, display, http_api, cli).

## Development

```bash
make help          # List all targets
make format        # Auto-format with clang-format
make format-check  # Check formatting (used in CI)
make lint          # Static analysis with cppcheck
make build         # Build both products
make build-matcha  # Build matcha-bowl only
make build-liquid  # Build liquid-machine only
make clean         # Remove build directories
```

CI runs on GitHub Actions (`.github/workflows/ci.yml`): format check → lint → build both products using `espressif/idf:v6.0` Docker image. Build artifacts are uploaded.

## Conventions

- C (not C++), ESP-IDF component structure
- Formatting: clang-format with `.clang-format` config (Linux brace style, 4-space indent, 100 col)
- GPIO pins defined as `BOARD_HAL_*_PIN` macros in `board_hal.h`
- Logging: `ESP_LOGI(TAG, ...)` with `static const char *TAG = "component_name"`
- Error handling: return `esp_err_t`, check with `ESP_ERROR_CHECK()` at init
- FreeRTOS tasks for polling loops, `esp_timer` for timing-critical ISR work
- NVS for persistent settings, cJSON for HTTP API JSON handling

## Schematics

Hardware schematics are in `<product>/schematic/`. Refer to these for GPIO mappings, I2C addresses, and peripheral wiring. The `board_hal.h` for each product documents the pin assignments derived from the schematic.
