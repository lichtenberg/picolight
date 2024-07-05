#ifndef _PICO_LASERPIXEL_H

class Pico_LaserPixel : public PicoPixel {

 public:

  // Constructor: number of LEDs, pin number, LED type
   Pico_LaserPixel(int8_t p);
   Pico_LaserPixel(void);
  ~Pico_LaserPixel();

    void begin(void);
    void show(void);
    void setPin(int8_t p);
    int8_t getPin(void);
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    void setPixelColor(uint16_t n, uint32_t c);
    uint32_t getPixelColor(uint16_t n) const;
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    uint32_t Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    uint16_t numPixels(void) const;
    void setBrightness(uint8_t);
    void clear(void);
private:
    int8_t pin;
    int pwmSlice;
    uint16_t pwmValue;

};

#endif
