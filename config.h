#pragma once

/*
 * RP2040-Touch-LCD-1.28-B
 *
 * Onboard: GC9A01 240x240 round LCD, CST816S touch, QMI8658 IMU.
 * The SH1.0 expansion connector brings out GPIO0 through GPIO5.
 * GPIO6-GPIO13, GPIO21-GPIO25, and GPIO29 are used by the LCD, touch,
 * IMU interrupt lines, backlight, and battery sense.
 */

/* Alarm output. Idle low, pulses high. Use GPIO0-GPIO5 only. */
#define ALARM_GPIO 0
#define ALARM_PULSE_MS 250
#define ALARM_HYSTERESIS_MG 15
#define ALARM_LIMIT_MG_DEFAULT 50
#define ALARM_LIMIT_MIN_MG 5
#define ALARM_LIMIT_MAX_MG 2000
#define ALARM_LIMIT_STEP_MG 5
#define ALARM_SETTLE_MS 1500

/* Set to 1 if taps on the left and right of the glass feel swapped. */
#define TOUCH_MIRROR_X 0

#define PIN_I2C_SDA 6
#define PIN_I2C_SCL 7
#define I2C_INST 1

#define PIN_LCD_DC 8
#define PIN_LCD_CS 9
#define PIN_LCD_SCK 10
#define PIN_LCD_MOSI 11
#define PIN_LCD_RST 13
#define PIN_LCD_BL 25
#define SPI_INST 1
#define LCD_SPI_HZ 40000000

#define PIN_TOUCH_INT 21
#define PIN_TOUCH_RST 22

#define QMI8658_ADDR_PRIMARY 0x6B
#define QMI8658_ADDR_SECONDARY 0x6A

/*
 * Accelerometer: ±2 g (16384 LSB/g, about 0.061 mg/LSB) so a footstep,
 * which is typically tens of mg on a table, is far above the noise.
 * The sensor runs at 500 Hz. Core 1 polls it at ACCEL_SAMPLE_HZ.
 * The hardware low-pass is left off so the impact is not smoothed away.
 */
#define ACCEL_SAMPLE_HZ 250
#define ACCEL_ODR_CODE 0x04

/*
 * Gravity-removal time constant is about 2^SHIFT / ACCEL_SAMPLE_HZ seconds.
 * Shift 6 is ~0.26 s at 250 Hz: a footstep (tens of ms) is kept, and a
 * slow tilt does not sit above the alarm line.
 */
#define BASELINE_SHIFT 6

/* One stored point per 100 ms, kept for 5 minutes. Each point is the
 * largest excursion inside that 100 ms, so a short footstep is not averaged
 * out when the 5-minute window is drawn into ~180 columns. */
#define TRACE_BIN_MS 100
#define TRACE_WINDOW_MS (5 * 60 * 1000)
#define TRACE_BINS (TRACE_WINDOW_MS / TRACE_BIN_MS)

/* Quiet vertical scale. The view widens when a larger spike is on screen. */
#define GRAPH_MIN_SCALE_MG 120

#define PEAK_HOLD_MS 2000

/* Settings live in the 4 KB sector at 1 MB. That page exists on the 16 MB
 * flash of this board and on smaller 2 MB parts as well. */
#define SETTINGS_FLASH_OFFSET (1024 * 1024)

#define PIN_IS_RESERVED(p)                                                     \
    ((p) == 6 || (p) == 7 || (p) == 8 || (p) == 9 || (p) == 10 || (p) == 11 || \
     (p) == 12 || (p) == 13 || (p) == 21 || (p) == 22 || (p) == 23 ||          \
     (p) == 24 || (p) == 25 || (p) == 29)

#if PIN_IS_RESERVED(ALARM_GPIO)
#error ALARM_GPIO is used by the LCD, touch, or IMU. Choose GPIO0-GPIO5.
#endif
#if (ALARM_GPIO) < 0 || (ALARM_GPIO) > 5
#error ALARM_GPIO must be GPIO0-GPIO5 on the SH1.0 expansion connector.
#endif
