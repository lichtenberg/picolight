#ifndef _PICOPIXEL_H_
#define _PICOPIXEL_H_

class PicoPixel {
public:
    // Standard constructor and destructor
    PicoPixel(void);
    virtual ~PicoPixel();

    // Virtual functions
    virtual void begin(void);
    virtual void show(void);
    virtual int8_t getPin(void);
    virtual void setPin(int8_t p);

    virtual void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
    virtual void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    virtual void setPixelColor(uint16_t n, uint32_t c);
    virtual uint32_t getPixelColor(uint16_t n) const;
    virtual uint32_t Color(uint8_t r, uint8_t g, uint8_t b);
    virtual uint32_t Color(uint8_t r, uint8_t g, uint8_t b, uint8_t w);
    virtual uint16_t numPixels(void) const;
    virtual void setBrightness(uint8_t);
    virtual void clear(void);

};


#endif
