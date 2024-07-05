#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"

#include "PicoPixel.h"
#include "PicoLaserPixel.h"

#define CONFIG_PWM      1

Pico_LaserPixel::Pico_LaserPixel(int8_t p)
{
    pin = p;

}

Pico_LaserPixel::Pico_LaserPixel(void)
{
}

Pico_LaserPixel::~Pico_LaserPixel()
{
}

void Pico_LaserPixel::begin(void)
{
    int aPin = pin;

    gpio_init(aPin);
#if CONFIG_PWM
    gpio_set_function(aPin, GPIO_FUNC_PWM);

    pwmSlice = pwm_gpio_to_slice_num(aPin);

    pwm_config config = pwm_get_default_config();
    // Set divider, reduces counter clock to sysclock/this value
    pwm_config_set_clkdiv(&config, 4.f);
    // Load the configuration into our PWM slice, and set it running.
    pwm_init(pwmSlice, &config, true);
#else
    gpio_set_dir(aPin, GPIO_OUT);
    gpio_put(aPin,1);
#endif
}

void Pico_LaserPixel::show(void)
{
#if CONFIG_PWM
    pwm_set_gpio_level(pin, pwmValue * pwmValue);
#else
    gpio_put(pin, (pwmValue > 80));
#endif
}

void Pico_LaserPixel::setPin(int8_t p)
{
    pin = p;
}

int8_t Pico_LaserPixel::getPin(void)
{
    return pin;
}

void Pico_LaserPixel::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{
    pwmValue = ((uint16_t) r + (uint8_t) g + (uint8_t) b) / 3;
}

void Pico_LaserPixel::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    pwmValue = ((uint16_t) r + (uint8_t) g + (uint8_t) b) / 3;
}

void Pico_LaserPixel::setPixelColor(uint16_t n, uint32_t c)
{
    pwmValue = ((c & 0xFF) + ((c >> 8) & 0xFF) + ((c >> 16) & 0xFF));
}

uint32_t Pico_LaserPixel::getPixelColor(uint16_t n) const
{
    return 0;
}

uint32_t Pico_LaserPixel::Color(uint8_t r, uint8_t g, uint8_t b)
{
    return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

uint32_t Pico_LaserPixel::Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
    return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

uint16_t Pico_LaserPixel::numPixels(void) const
{
    return 1;
}
void Pico_LaserPixel::setBrightness(uint8_t)
{
}

void Pico_LaserPixel::clear(void)
{
    pwmValue = 0;
}


