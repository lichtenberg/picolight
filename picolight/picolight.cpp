//
// PicoLight
// (C) 2021 Mitch Lichtenberg (both of them).
//

#ifndef REALPANEL
#define REALPANEL 0
#endif

#ifndef MINIPANEL
#define MINIPANEL 0
#endif

#ifndef TRIANGLES
#define TRIANGLES 1
#endif


#if (MINIPANEL + TRIANGLES + REALPANEL) != 1
#error "You must define exactly one of MINIPANEL, TRIANGLES, REALPANEL"
#endif


// Use this command to create archive
// zip -r Lightshow.zip ./Lightshow -x '*.git*'


#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "ws2812.pio.h"

#include "PicoNeoPixel.h"
#include "AlaLedRgb.h"

#include "xtimer.h"

#define PAL_RGB         64
#define PAL_RAINBOW     65
#define PAL_RAINBOWSTRIPE 66
#define PAL_PARTY       67
#define PAL_HEAT        68
#define PAL_FIRE        69
#define PAL_COOL        70
#define PAL_WHITE       71
#define PAL_RED         80
#define PAL_GREEN       82
#define PAL_BLUE        84


int debug = 0;

#define MSGSIZE         16
#define STATE_SYNC1     0
#define STATE_SYNC2     1
#define STATE_RX        2

int rxstate = STATE_SYNC1;
int rxcount = 0;

typedef struct lsmessage_s {
//    uint8_t     ls_sync[2];
    uint16_t    ls_reserved;
    uint16_t    ls_anim;
    uint16_t    ls_speed;
    uint16_t    ls_option;
    uint32_t    ls_color;
    uint32_t    ls_strips;
} lsmessage_t;

lsmessage_t message;

extern int displayInit(const char *name);
extern int displayUpdate(void);



ws2812pio_t wsp;

/*  *********************************************************************
    *  Timer Stuff.  Macros are in xtimer.h
    ********************************************************************* */

// Our sense of "now".  It's just a number that counts at 1KHz.
extern "C" {
    volatile xtimer_t __now = 0;
}

static xtimer_t blinky_timer;
static xtimer_t displayUpdateTimer;
static char blinky_onoff = 0;

#define PIN_LED 25

// Flow control for our special dual-Arduino setup that
// uses a Pro Micro to process message the Uno to run the pixels
#define PIN_DATA_AVAIL  8                 // input, '1' if there is data
#define PIN_SEND_OK     9                 // output, '1' to send data


/*  *********************************************************************
    *  LED Strips
    ********************************************************************* */

#if (REALPANEL || MINIPANEL)
#define MAXPHYSICALSTRIPS 12
#define MAXLOGICALSTRIPS 31
#endif
#if (TRIANGLES)
#define MAXPHYSICALSTRIPS 5
#define MAXLOGICALSTRIPS 5
#endif


/*  *********************************************************************
    *  Global Variables
    ********************************************************************* */

#if REALPANEL
static const char *configName = "PANEL";
#endif
#if MINIPANEL
static const char *configName = "MINI";
#endif
#if TRIANGLES
static const char *configName = "TRIANGLE";
#endif

//
// strip stack - specifies the order to run the animation renders
//

int stripStack[MAXLOGICALSTRIPS];

//
// This array contains the pin numbers that
// correspond to each LED strip.
//

#define PORT_A1         14
#define PORT_A2         15
#define PORT_A3         16
#define PORT_A4         17
#define PORT_A5         18
#define PORT_A6         19
#define PORT_A7         20
#define PORT_A8         21

#define PORT_B1         6
#define PORT_B2         7
#define PORT_B3         8
#define PORT_B4         9
#define PORT_B5         10
#define PORT_B6         11
#define PORT_B7         12
#define PORT_B8         13

// This struct describes a physical strip
typedef struct PhysicalStrip_s {
    uint8_t pin;                        // Pin number for strips
    uint8_t length;                     // Number of actual pixels
    Pico_NeoPixel *neopixels;       // Neopixel object.
} PhysicalStrip_t;

#if (MINIPANEL || REALPANEL)
enum {
    PHYS_SPOKE1 = 0,
    PHYS_SPOKE2,
    PHYS_SPOKE3,
    PHYS_SPOKE4,
    PHYS_SPOKE5,
    PHYS_SPOKE6,
    PHYS_SPOKE7,
    PHYS_SPOKE8,
    PHYS_RING,
    PHYS_TOP,
    PHYS_BOTTOM,
    PHYS_RINGLETS,
    PHYS_EOT = 0xFF
};
#endif
#if TRIANGLES
enum {
    PHYS_TRI1 = 0,
    PHYS_TRI2,
    PHYS_TRI3,
    PHYS_TRI4,
    PHYS_NEON,
    PHYS_EOT = 0xFF
};
#endif


#if MINIPANEL
#pragma message "Building for MINI-ART config"
PhysicalStrip_t physicalStrips[MAXPHYSICALSTRIPS] = {
    [PHYS_SPOKE1] = { .pin = PORT_A1,   .length = 30 },         // SPOKE1
    [PHYS_SPOKE2] = { .pin = PORT_A2,   .length = 30 },         // SPOKE2
    [PHYS_SPOKE3] = { .pin = PORT_A3,   .length = 30 },         // SPOKE3
    [PHYS_SPOKE4] = { .pin = PORT_B4,   .length = 30 },         // SPOKE4
    [PHYS_SPOKE5] = { .pin = PORT_B2,   .length = 30 },         // SPOKE5
    [PHYS_SPOKE6] = { .pin = PORT_B3,   .length = 30 },         // SPOKE6
    [PHYS_SPOKE7] = { .pin = PORT_A4,   .length = 30 },         // SPOKE7
    [PHYS_SPOKE8] = { .pin = PORT_A8,   .length = 30 },         // SPOKE8
    [PHYS_RING] =   { .pin = PORT_A5,   .length = 60 },         // RING
    [PHYS_TOP]    = { .pin = PORT_B5,   .length = 160 },        // TOP
    [PHYS_BOTTOM] = { .pin = PORT_B6,   .length = 160 },        // BOTTOM
    [PHYS_RINGLETS] = { .pin = PORT_A7, .length = 128 },        // RINGLETS
};
#endif
#if REALPANEL
#pragma message "Building for REALPANEL config"
PhysicalStrip_t physicalStrips[MAXPHYSICALSTRIPS] = {
    [PHYS_SPOKE1] = { .pin = PORT_A2,   .length = 30 },         // SPOKE1
    [PHYS_SPOKE2] = { .pin = PORT_A3,   .length = 30 },         // SPOKE2
    [PHYS_SPOKE3] = { .pin = PORT_A4,   .length = 30 },         // SPOKE3
    [PHYS_SPOKE4] = { .pin = PORT_B5,   .length = 30 },         // SPOKE4
    [PHYS_SPOKE5] = { .pin = PORT_B6,   .length = 30 },         // SPOKE5
    [PHYS_SPOKE6] = { .pin = PORT_B7,   .length = 30 },         // SPOKE6
    [PHYS_SPOKE7] = { .pin = PORT_B8,   .length = 30 },         // SPOKE7
    [PHYS_SPOKE8] = { .pin = PORT_A1,   .length = 30 },         // SPOKE8
    [PHYS_RING] =   { .pin = PORT_A5,   .length = 60 },         // RING
    [PHYS_TOP]    = { .pin = PORT_A6,   .length = 130 },        // TOP
    [PHYS_BOTTOM] = { .pin = PORT_B4,   .length = 130 },        // BOTTOM
    [PHYS_RINGLETS] = { .pin = PORT_A7, .length = 128 },        // RINGLETS
};
#endif

#if TRIANGLES
PhysicalStrip_t physicalStrips[MAXPHYSICALSTRIPS] = {
    [PHYS_TRI1] = { .pin = PORT_A1,   .length = 5 },
    [PHYS_TRI2] = { .pin = PORT_A2,   .length = 5 },
    [PHYS_TRI3] = { .pin = PORT_A3,   .length = 5 },
    [PHYS_TRI4] = { .pin = PORT_A4,   .length = 5 },
    [PHYS_NEON] = { .pin = PORT_A5,   .length = 96 },
};
#endif

//
// OK, here are the "logical" strips, which can be composed from pieces of phyiscal strips.
//

//
// Constants to define each spoke
// These are "bit masks", meaning there is exactly one bit set in each of these
// numbers.  By using a bit mask, we can conveniently represent sets of small numbers
// by treating each bit to mean "I am in this set".
//
// So, if you have a binary nunber 00010111, then bits 0,1,2, and 4 are set.
// You could therefore use the binary number 00010111, which is 11 in decimal,
// to mean "the set of 0, 1, 2, and 4"
//
// Define some shorthands for sets of strips.  We can add as many
// as we want here.
//


//#define ALLSTRIPS (((uint32_t)1 << MAXLOGICALSTRIPS)-1)

#if (MINIPANEL || REALPANEL)
#define ALLSTRIPS (((uint32_t)1 << 19)-1)
enum {
    LOG_SPOKE1 = 0, LOG_SPOKE2, LOG_SPOKE3, LOG_SPOKE4,
    LOG_SPOKE5, LOG_SPOKE6, LOG_SPOKE7, LOG_SPOKE8,

    LOG_RING,
    LOG_TOPHALF,
    LOG_BOTTOMHALF,
    LOG_RINGLET1,LOG_RINGLET2,LOG_RINGLET3,LOG_RINGLET4,
    LOG_RINGLET5,LOG_RINGLET6,LOG_RINGLET7,LOG_RINGLET8,

    LOG_TOP, LOG_RIGHT, LOG_BOTTOM, LOG_LEFT,

    LOG_ULCORNER, LOG_URCORNER, LOG_LRCORNER, LOG_LLCORNER,
    LOG_ULBOX, LOG_URBOX, LOG_LRBOX, LOG_LLBOX
};
#endif
#if TRIANGLES
#define ALLSTRIPS (((uint32_t)1 << 5)-1)
enum {
    LOG_TRI1 = 0, LOG_TRI2, LOG_TRI3, LOG_TRI4, LOG_NEON
};
#endif


//
// Additional sets we might want to define:
// LEFTSPOKES, RIGHTSPOKES, TOPSPOKES, BOTTOMSPOKES
//

typedef struct LogicalSubStrip_s {
    uint8_t physical;
    uint8_t startingLed;
    uint8_t numLeds;
    uint8_t reverse;
} LogicalSubStrip_t;

typedef struct LogicalStrip_s {
//    uint8_t     physical;               // which physical strip (index into array)
//    uint8_t     startingLed;            // starting LED within this strip
//    uint8_t     numLeds;                // number of LEDs (0 for whole strip)
//    uint8_t     reverse;                // true if order is reversed
    const LogicalSubStrip_t *subStrips;
    AlaLedRgb *alaStrip;                // ALA object we created.
} LogicalStrip_t;


//
// Definitions of each logical strip
//
#if (MINIPANEL || REALPANEL)
const LogicalSubStrip_t substrip_SPOKE1[] = {
    {.physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE2[] = {
    {.physical = PHYS_SPOKE2, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE3[] = {
    {.physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE4[] = {
    {.physical = PHYS_SPOKE4, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE5[] = {
    {.physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE6[] = {
    {.physical = PHYS_SPOKE6, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE7[] = {
    {.physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_SPOKE8[] = {
    {.physical = PHYS_SPOKE8, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};

const LogicalSubStrip_t substrip_RING[] = {
    {.physical = PHYS_RING, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TOPHALF[] = {
    {.physical = PHYS_TOP, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_BOTTOMHALF[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};

const LogicalSubStrip_t substrip_RINGLET1[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 0, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET2[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 16, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET3[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 32, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET4[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 48, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET5[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 64, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET6[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 80, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET7[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 96, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RINGLET8[] = {
    {.physical = PHYS_RINGLETS, .startingLed = 112, .numLeds = 16, .reverse = 0},
    {.physical = PHYS_EOT}
};


// Substrips: Edges of Perimeter
#if MINIPANEL
const LogicalSubStrip_t substrip_LEFT[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 40, .reverse = 1},
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 40, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TOP[] = {
    {.physical = PHYS_TOP, .startingLed = 40, .numLeds = 80, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RIGHT[] = {
    {.physical = PHYS_TOP, .startingLed = 120, .numLeds = 40, .reverse = 0},
    {.physical = PHYS_BOTTOM, .startingLed = 120, .numLeds = 40, .reverse = 1},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_BOTTOM[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 40, .numLeds = 80, .reverse = 1},
    {.physical = PHYS_EOT}
};

// Substrips: Corners of Perimeter
const LogicalSubStrip_t substrip_ULCORNER[] = {
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 80, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_URCORNER[] = {
    {.physical = PHYS_TOP, .startingLed = 80, .numLeds = 80, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LRCORNER[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 80, .numLeds = 80, .reverse = 1},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LLCORNER[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 80, .reverse = 1},
    {.physical = PHYS_EOT}
};

    
// Substrips: Boxes
const LogicalSubStrip_t substrip_ULBOX[] = {
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 80, .reverse = 0},
    {.physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_URBOX[] = {
    {.physical = PHYS_TOP,    .startingLed = 80, .numLeds = 80, .reverse = 0},
    {.physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LRBOX[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 80, .numLeds = 80, .reverse = 1},
    {.physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LLBOX[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 80, .reverse = 1},
    {.physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
#endif

#if REALPANEL
const LogicalSubStrip_t substrip_LEFT[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 32, .reverse = 1},
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 30, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TOP[] = {
    {.physical = PHYS_TOP, .startingLed = 30, .numLeds = 65, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_RIGHT[] = {
    {.physical = PHYS_TOP, .startingLed = 95, .numLeds = 30, .reverse = 0},
    {.physical = PHYS_BOTTOM, .startingLed = 97, .numLeds = 32, .reverse = 1},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_BOTTOM[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 32, .numLeds = 65, .reverse = 1},
    {.physical = PHYS_EOT}
};

// Substrips: Corners of Perimeter
const LogicalSubStrip_t substrip_ULCORNER[] = {
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 63, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_URCORNER[] = {
    {.physical = PHYS_TOP, .startingLed = 63, .numLeds = 62, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LRCORNER[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 66, .numLeds = 63, .reverse = 1},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LLCORNER[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 66, .reverse = 1},
    {.physical = PHYS_EOT}
};
    
// Substrips: Boxes
const LogicalSubStrip_t substrip_ULBOX[] = {
    {.physical = PHYS_TOP,    .startingLed = 0, .numLeds = 63, .reverse = 0},
    {.physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_URBOX[] = {
    {.physical = PHYS_TOP,    .startingLed = 63, .numLeds = 62, .reverse = 0},
    {.physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE1, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LRBOX[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 66, .numLeds = 63, .reverse = 1},
    {.physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE3, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_LLBOX[] = {
    {.physical = PHYS_BOTTOM, .startingLed = 0, .numLeds = 66, .reverse = 1},
    {.physical = PHYS_SPOKE7, .startingLed = 0, .numLeds = 0, .reverse = 1},
    {.physical = PHYS_SPOKE5, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
#endif
#endif


#if (MINIPANEL || REALPANEL)
LogicalStrip_t logicalStrips[MAXLOGICALSTRIPS] = {
    // Spokes
    [LOG_SPOKE1] = { .subStrips = substrip_SPOKE1 },
    [LOG_SPOKE2] = { .subStrips = substrip_SPOKE2 },
    [LOG_SPOKE3] = { .subStrips = substrip_SPOKE3 },
    [LOG_SPOKE4] = { .subStrips = substrip_SPOKE4 },
    [LOG_SPOKE5] = { .subStrips = substrip_SPOKE5 },
    [LOG_SPOKE6] = { .subStrips = substrip_SPOKE6 },
    [LOG_SPOKE7] = { .subStrips = substrip_SPOKE7 },
    [LOG_SPOKE8] = { .subStrips = substrip_SPOKE8 },

    // Ring
    [LOG_RING]   = { .subStrips = substrip_RING },

    // Top and bottom halves of the perimeter
    [LOG_TOPHALF]    = { .subStrips = substrip_TOPHALF },
    [LOG_BOTTOMHALF] = { .subStrips = substrip_BOTTOMHALF },

    // Ringlets, all the rings are on one physical strip
    [LOG_RINGLET1] = { .subStrips = substrip_RINGLET1 },
    [LOG_RINGLET2] = { .subStrips = substrip_RINGLET2 },
    [LOG_RINGLET3] = { .subStrips = substrip_RINGLET3 },
    [LOG_RINGLET4] = { .subStrips = substrip_RINGLET4 },
    [LOG_RINGLET5] = { .subStrips = substrip_RINGLET5 },
    [LOG_RINGLET6] = { .subStrips = substrip_RINGLET6 },
    [LOG_RINGLET7] = { .subStrips = substrip_RINGLET7 },
    [LOG_RINGLET8] = { .subStrips = substrip_RINGLET8 },

    // Horizontal and vertical edges of the perimeter
    [LOG_TOP] = { .subStrips = substrip_TOP },
    [LOG_RIGHT] = { .subStrips = substrip_RIGHT },
    [LOG_BOTTOM] = { .subStrips = substrip_BOTTOM },
    [LOG_LEFT] = { .subStrips = substrip_LEFT },

    // Corners of the perimeter
    [LOG_ULCORNER] = { .subStrips = substrip_ULCORNER },
    [LOG_URCORNER] = { .subStrips = substrip_URCORNER },
    [LOG_LRCORNER] = { .subStrips = substrip_LRCORNER },
    [LOG_LLCORNER] = { .subStrips = substrip_LLCORNER },

    // Boxes, combining perimeter corners and spokes
    [LOG_ULBOX] = { .subStrips = substrip_ULBOX },
    [LOG_URBOX] = { .subStrips = substrip_URBOX },
    [LOG_LRBOX] = { .subStrips = substrip_LRBOX },
    [LOG_LLBOX] = { .subStrips = substrip_LLBOX },

};
#endif

#if (TRIANGLES)
const LogicalSubStrip_t substrip_TRI1[] = {
    {.physical = PHYS_TRI1, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TRI2[] = {
    {.physical = PHYS_TRI2, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TRI3[] = {
    {.physical = PHYS_TRI3, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_TRI4[] = {
    {.physical = PHYS_TRI4, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};
const LogicalSubStrip_t substrip_NEON[] = {
    {.physical = PHYS_NEON, .startingLed = 0, .numLeds = 0, .reverse = 0},
    {.physical = PHYS_EOT}
};

LogicalStrip_t logicalStrips[MAXLOGICALSTRIPS] = {
    // Spokes
    [LOG_TRI1] = { .subStrips = substrip_TRI1 },
    [LOG_TRI2] = { .subStrips = substrip_TRI2 },
    [LOG_TRI3] = { .subStrips = substrip_TRI3 },
    [LOG_TRI4] = { .subStrips = substrip_TRI4 },
    [LOG_NEON] = { .subStrips = substrip_NEON },
};
#endif



/*  *********************************************************************
    *  Palettes
    ********************************************************************* */

// Special "None" palette used for passing direct colors in.
AlaColor alaPalNone_[] = { 0 };
AlaPalette alaPalNone = { 0, alaPalNone_ };

AlaColor alaPalWhite_[] = { 0xFFFFFF };
AlaPalette alaPalWhite = { 1, alaPalWhite_ };

AlaColor alaPalRed_[] = { 0xFF0000 };
AlaPalette alaPalRed = { 1, alaPalRed_ };

AlaColor alaPalGreen_[] = { 0x00FF00 };
AlaPalette alaPalGreen = { 1, alaPalGreen_ };

AlaColor alaPalBlue_[] = { 0x0000FF };
AlaPalette alaPalBlue = { 1, alaPalBlue_ };


void pushStrip(int strip)
{
    int i;
    
    // First remove the strip from the stack

    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        if (stripStack[i] == strip) {
            break;
        }
    }

    if (i != MAXLOGICALSTRIPS) {
        // Move everyone else up
        for (; i < MAXLOGICALSTRIPS-1; i++) {
            stripStack[i] = stripStack[i+1];
        }

        // Since we removed one, put a -1 at the end in its place.
        stripStack[MAXLOGICALSTRIPS-1] = -1;
    }

    // OK, now insert the new one at the top of the stack
    for (i = MAXLOGICALSTRIPS-1; i > 0; i--) {
        stripStack[i] = stripStack[i-1];
    }
    stripStack[0] = strip;
}
     



/*  *********************************************************************
    *  setAnimation(strips, animation, speed, palette)
    *  
    *  Sets a set of strips to a particular animation.  The list of
    *  strips to be set is passed in as a "bitmask", which is a
    *  number where each bit in the number represesents a particular
    *  strip.  If bit 0 is set, we will operate on strip 0,  Bit 1 means
    *  strip 1, and so on.   Therefore, if you pass in "10" decimal
    *  for "strips", in binary that is 00001010, so setStrips will
    *  set the animation for strips 1 and 3.  Using a bitmask is a 
    *  convenient way to pass a set of small numbers as a single
    *  value.
    ********************************************************************* */

void setAnimation(uint32_t strips, int animation, int speed, unsigned int direction,
                  unsigned int option, 
                  AlaPalette palette, AlaColor color)
{
    int i;

    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((strips & ((uint32_t)1 << i)) != 0) {
            logicalStrips[i].alaStrip->forceAnimation(animation, speed, direction, option, palette, color);
            pushStrip(i);
        }
    }
}


/*  *********************************************************************
    *  setup()
    *  
    *  Arduino SETUP routine - perform once-only initialization.
    ********************************************************************* */

void setup()
{
    int i;

    //
    // Set up our "timer", which lets us check to see how much time
    // has passed.  We might not use it for much, but it is handy to have.
    // For now, we'll mostly use it to blink the LED on the Arduino
    //

    TIMER_UPDATE();                 // remember current time
    TIMER_SET(blinky_timer,500);    // set timer for first blink

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);


    //
    // Initialize all of the physical strips
    //

    ws2812_program_init(&wsp, pio0, 0, 800000);
    
    for (i = 0; i < MAXPHYSICALSTRIPS; i++) {
        physicalStrips[i].neopixels =
            new Pico_NeoPixel(&wsp, physicalStrips[i].pin, physicalStrips[i].length, NEO_GRB);
        if (!physicalStrips[i].neopixels) printf("Out of memory creating physcial strips\n");
        physicalStrips[i].neopixels->begin();
    }

    // Now init the logical strips, but map them 1:1 to the physical ones.
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        const LogicalSubStrip_t *substrip;
        uint8_t startingLed, numLeds;
        // Create a new logical strip
        AlaLedRgb *alaLed = new AlaLedRgb;
        // If the logical strip's length is 0, it's the whole physical strip
        if (!alaLed) printf("Out of memory creating logical strips\n");

        substrip = logicalStrips[i].subStrips;

        while (substrip->physical != PHYS_EOT) {
            if (substrip->numLeds == 0) {
                startingLed = 0;
                numLeds = physicalStrips[substrip->physical].length;
            } else {
                startingLed = substrip->startingLed;
                numLeds = substrip->numLeds;
            }
            alaLed->addSubStrip(startingLed,
                                numLeds,
                                substrip->reverse,
                                physicalStrips[substrip->physical].neopixels);
            substrip++;
        }
        logicalStrips[i].alaStrip = alaLed;
    }

    // OK, go back and "begin" all of the strips.  This allocates
    // the underlying memory for the pixels.
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        logicalStrips[i].alaStrip->begin();
    }

    // Init the strip stack
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        stripStack[i] = -1;
    }

    // By default, run the "idle white" animation on all strips.
    // We can change this later if we want the art exhibit to start quietly.
    setAnimation(ALLSTRIPS, ALA_IDLEWHITE, 1000, 0, 0, alaPalRgb, 0);


}



/*  *********************************************************************
    *  blinky()
    *  
    *  Blink the onboard LED at 1hz
    ********************************************************************* */

static void blinky(void)
{
    if (TIMER_EXPIRED(blinky_timer)) {
        blinky_onoff = !blinky_onoff;
        gpio_put(PIN_LED, blinky_onoff);
        TIMER_SET(blinky_timer,500);
    }
}


/*  *********************************************************************
    *  loop()
    *  
    *  This is the main Arduino loop.  Handle characters 
    *  received from the Basic Micro
    ********************************************************************* */

void handleMessage(lsmessage_t *msgp)
{

    uint32_t stripMask;
    int animation, speed;
    uint32_t palette;
    unsigned int direction;
    unsigned int option;
    AlaPalette ap;
    AlaColor color = 0;

    // 
    TIMER_CLEAR(displayUpdateTimer);

    stripMask = msgp->ls_strips;
    animation = msgp->ls_anim;
    speed = msgp->ls_speed;
    // Use the top bit fo the animation to indicate the direction.
    direction = (animation & 0x8000) ? 1 : 0;
    animation = (animation & 0x7FFF);
    palette = msgp->ls_color;
    option = msgp->ls_option;

    if (palette & 0x1000000) {
        ap = alaPalNone;
        color = (palette & 0x00FFFFFF);
    } else {
        switch (palette) {
            default:
            case PAL_RGB:
                ap = alaPalRgb;
                break;
            case PAL_RAINBOW:
                ap = alaPalRainbow;
                break;
            case PAL_RAINBOWSTRIPE:
                ap = alaPalRainbowStripe;
                break;
            case PAL_PARTY:
                ap = alaPalParty;
                break;
            case PAL_HEAT:
                ap = alaPalHeat;
                break;
            case PAL_FIRE:
                ap = alaPalFire;
                break;
            case PAL_COOL:
                ap = alaPalCool;
                break;
            case PAL_WHITE:
                ap = alaPalWhite;
                break;
            case PAL_RED:
                ap = alaPalRed;
                break;
            case PAL_GREEN:
                ap = alaPalGreen;
                break;
            case PAL_BLUE:
                ap = alaPalBlue;
                break;
        }
    }

    setAnimation(stripMask, animation, speed, direction, option, ap, color);
    
}


void checkForMessage(void)
{
    uint8_t *msgPtr = (uint8_t *) &message;
    int c;
    unsigned char b;

    for (;;) {
        c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            break;
        }

        b = (unsigned char) c;

        switch (rxstate) {
            case STATE_SYNC1: 
                if (b == 0x02) rxstate = STATE_SYNC2;
                break;
            case STATE_SYNC2:
                if (b == 0xAA) {
                    rxcount = 0;
                    rxstate = STATE_RX;
                }
                else {
                    rxstate = STATE_SYNC1;
                }
                break;
            case STATE_RX:
                msgPtr[rxcount] = b;
                rxcount++;
                if (rxcount == MSGSIZE) {
                    rxstate = STATE_SYNC1;
                    handleMessage(&message);
                }
                break;
        }
    }
}

void loop()
{
    int i;

    // Update the current number of milliseconds since start
    TIMER_UPDATE();

    // Blink the LED.
    blinky();

    // 
    // Check for data from our other Arduino, which is doing flow control for us.
    // 

    checkForMessage();

    if (TIMER_EXPIRED(displayUpdateTimer)) {
        displayInit(configName);
        displayUpdate();
        TIMER_SET(displayUpdateTimer, 5000);
    }

    //
    // Run animations on all strips
    //
    // First compute new pixels on the LOGICAL Strips
#if 0
    for (i = MAXLOGICALSTRIPS-1; i >= 0; i--) {
        if (stripStack[i] != -1) {
            logicalStrips[stripStack[i]].alaStrip->runAnimation();
        }
    }
#else
    for (i = 0; i < MAXLOGICALSTRIPS; i++) {
        logicalStrips[i].alaStrip->runAnimation();
    }
#endif

    // Now send the data to the PHYSICAL strips
    for (i = 0; i < MAXPHYSICALSTRIPS; i++) {
        physicalStrips[i].neopixels->show();
    }


}

const char *build_date = __DATE__;


int main()
{
    stdio_init_all();

    // Initialize the I2C interface
    i2c_init(i2c_default, 400 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);

    displayInit(configName);
    displayUpdate();

    setup();
    TIMER_SET(displayUpdateTimer, 5000);
    for (;;) {
        loop();
    }
}
