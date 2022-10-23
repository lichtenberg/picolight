#ifndef AlaLedRgb_h
#define AlaLedRgb_h

#include "Ala.h"

#include "PicoNeoPixel.h"

// This represents a piece of a Neopixel Strip.  We can have more than
// one AlaSubStrip associated with an AlaLedRgb to spread the actual
// pixels out among a bunch of physical strips.
typedef struct AlaSubStrip_s {
    int startingLed;
    int numLeds;
    bool reverse;
    Pico_NeoPixel *pixels;
} AlaSubStrip;

#define MAXSUBSTRIPS 8

/**
 *  AlaLedRgb can be used to drive a single or multiple RGB leds to perform animations.
 */
class AlaLedRgb
{

public:

    AlaLedRgb();
    ~AlaLedRgb();

    /**
    * Initializes a substrip, adding set of LEDs from a physical strip to this AlaLedRgb
    */

    void addSubStrip(int startingLed, int numLeds, bool reverse, Pico_NeoPixel *pixels);
    void begin(void);

    /**
    * Sets the maximum brightness level.
    */
    void setBrightness(AlaColor maxOut);

    /**
    * Sets the maximum refresh rate in Hz (default value is 50 Hz).
    * May be useful to reduce flickering in some cases.
    */
    void setRefreshRate(int refreshRate);

    int getCurrentRefreshRate();

    void setAnimationSpeed(long speed);

//    void setAnimation(int animation, long speed, unsigned int direction, AlaColor color);
    void setAnimation(int animation, long speed, unsigned int direction, unsigned int option, AlaPalette palette, AlaColor color);

//    void forceAnimation(int animation, long speed, unsigned int direction, AlaColor color);
    void forceAnimation(int animation, long speed, unsigned int direction, unsigned int option, AlaPalette palette, AlaColor color);

//    void setAnimation(AlaSeq animSeq[]);
    int getAnimation();

    bool runAnimation();



private:

    void setAnimationFunc(int animation);
    void stop();
    void on();
    void off();
    void blink();
    void blinkAlt();
    void sparkle();
    void sparkle2();
    void strobo();
    void cycleColors();

    // New animations for art project
    void soundPulse();
    void idleWhite();
    void onePixel();
    void pixelLine();
    void grow();
    void shrink();
    void pixelMarch();

    void pixelShiftRight();
    void pixelShiftLeft();
    void pixelBounce();
    void pixelSmoothShiftRight();
    void pixelSmoothShiftLeft();
    void comet();
    void cometCol();
    void pixelSmoothBounce();
    void larsonScanner();
    void larsonScanner2();

    void fadeIn();
    void fadeOut();
    void fadeInOut();
    void glow();
    void plasma();
    void fadeColors();
    void pixelsFadeColors();
    void fadeColorsLoop();

    void movingBars();
    void movingGradient();

    void bouncingBalls();
    void bubbles();

    // Logical Strip Info
    AlaColor *leds; // array to store leds brightness values

    // Physical Strip Info
    int numSubStrips;
    AlaSubStrip subStrips[MAXSUBSTRIPS];

    int numLeds;

    int animation;
    long speed;
    unsigned int option;
    unsigned int direction;
    AlaColor singleColor;
    AlaPalette palette;
    AlaSeq *animSeq;
    int animSeqLen;
    long animSeqDuration;

    void (AlaLedRgb::*animFunc)();
    AlaColor maxOut;
    int refreshMillis;
    int refreshRate;   // current refresh rate
    unsigned long animStartTime;
    unsigned long animSeqStartTime;
    unsigned long lastRefreshTime;
    unsigned long animSeqCount;

    float *pxPos;
    float *pxSpeed;

    bool findPixel(int idx, Pico_NeoPixel **strip, int *whichLed);

};


#endif
