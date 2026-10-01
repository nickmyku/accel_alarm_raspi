#include "gc9a01.h"

#include "config.h"

#include "hardware/pwm.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

/*
 * Panel bring-up for the GC9A01A on the Waveshare RP2040-Touch-LCD-1.28.
 * Register sequence matches the sequence shipped with that board.
 */

static spi_inst_t *lcd_spi(void) {
    return SPI_INST == 1 ? spi1 : spi0;
}

static void lcd_cmd(uint8_t c) {
    gpio_put(PIN_LCD_DC, 0);
    spi_write_blocking(lcd_spi(), &c, 1);
}

static void lcd_data(const uint8_t *bytes, size_t n) {
    if (n == 0) {
        return;
    }
    gpio_put(PIN_LCD_DC, 1);
    spi_write_blocking(lcd_spi(), bytes, n);
}

#define S0(c, dly) \
    { (uint8_t)(c), 0, (uint16_t)(dly), {0} }
#define S(c, dly, ...) \
    { (uint8_t)(c), (uint8_t)sizeof((uint8_t[]){__VA_ARGS__}), (uint16_t)(dly), {__VA_ARGS__} }

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint16_t delay_ms;
    uint8_t data[12];
} lcd_step_t;

static const lcd_step_t k_init[] = {
    S0(0xEF, 0),
    S(0xEB, 0, 0x14),
    S0(0xFE, 0),
    S0(0xEF, 0),
    S(0xEB, 0, 0x14),
    S(0x84, 0, 0x40),
    S(0x85, 0, 0xFF),
    S(0x86, 0, 0xFF),
    S(0x87, 0, 0xFF),
    S(0x88, 0, 0x0A),
    S(0x89, 0, 0x21),
    S(0x8A, 0, 0x00),
    S(0x8B, 0, 0x80),
    S(0x8C, 0, 0x01),
    S(0x8D, 0, 0x01),
    S(0x8E, 0, 0xFF),
    S(0x8F, 0, 0xFF),
    S(0xB6, 0, 0x00, 0x20),
    S(0x36, 0, 0x08),
    S(0x3A, 0, 0x05),
    S(0x90, 0, 0x08, 0x08, 0x08, 0x08),
    S(0xBD, 0, 0x06),
    S(0xBC, 0, 0x00),
    S(0xFF, 0, 0x60, 0x01, 0x04),
    S(0xC3, 0, 0x13),
    S(0xC4, 0, 0x13),
    S(0xC9, 0, 0x22),
    S(0xBE, 0, 0x11),
    S(0xE1, 0, 0x10, 0x0E),
    S(0xDF, 0, 0x21, 0x0C, 0x02),
    S(0xF0, 0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A),
    S(0xF1, 0, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F),
    S(0xF2, 0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A),
    S(0xF3, 0, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F),
    S(0xED, 0, 0x1B, 0x0B),
    S(0xAE, 0, 0x77),
    S(0xCD, 0, 0x63),
    S(0x70, 0, 0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03),
    S(0xE8, 0, 0x34),
    S(0x62, 0, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70),
    S(0x63, 0, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70),
    S(0x64, 0, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07),
    S(0x66, 0, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00),
    S(0x67, 0, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98),
    S(0x74, 0, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00),
    S(0x98, 0, 0x3E, 0x07),
    S0(0x35, 0),
    S0(0x21, 0),
    S0(0x11, 120),
    S0(0x29, 20),
};

static void backlight_on(void) {
    gpio_set_function(PIN_LCD_BL, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(PIN_LCD_BL);
    pwm_set_clkdiv(slice, 50.0f);
    pwm_set_wrap(slice, 100);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(PIN_LCD_BL), 90);
    pwm_set_enabled(slice, true);
}

static void set_window(int x0, int y0, int x1, int y1) {
    uint8_t xs[4] = {(uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t ys[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    lcd_cmd(0x2A);
    lcd_data(xs, 4);
    lcd_cmd(0x2B);
    lcd_data(ys, 4);
    lcd_cmd(0x2C);
}

void gc9a01_init(void) {
    gpio_init(PIN_LCD_DC);
    gpio_init(PIN_LCD_CS);
    gpio_init(PIN_LCD_RST);
    gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    gpio_put(PIN_LCD_CS, 1);
    gpio_put(PIN_LCD_DC, 1);

    spi_init(lcd_spi(), LCD_SPI_HZ);
    spi_set_format(lcd_spi(), 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_MOSI, GPIO_FUNC_SPI);

    gpio_put(PIN_LCD_RST, 1);
    sleep_ms(20);
    gpio_put(PIN_LCD_RST, 0);
    sleep_ms(20);
    gpio_put(PIN_LCD_RST, 1);
    sleep_ms(120);
    /* This panel is the only device on SPI1. The board demo holds CS low. */
    gpio_put(PIN_LCD_CS, 0);

    for (size_t i = 0; i < sizeof k_init / sizeof k_init[0]; i++) {
        lcd_cmd(k_init[i].cmd);
        lcd_data(k_init[i].data, k_init[i].len);
        if (k_init[i].delay_ms) {
            sleep_ms(k_init[i].delay_ms);
        }
    }
    backlight_on();
}

void gc9a01_flush(const uint16_t *fb, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }
    set_window(0, 0, width - 1, height - 1);
    gpio_put(PIN_LCD_DC, 1);
    spi_write_blocking(lcd_spi(), (const uint8_t *)fb, (size_t)width * (size_t)height * 2);
}
