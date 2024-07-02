//
// PicoLight
// (C) 2021,2022 Mitch Lichtenberg (both of them).
//



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

int debug = 0;

/*  *********************************************************************
    *  Global State
    ********************************************************************* */

#define GSTATE_INIT     0               // Need to program tables
#define GSTATE_READY    1               // ready to play.

int globalState = GSTATE_INIT;

/*  *********************************************************************
    *  Receive state machine
    ********************************************************************* */

#define MSGSIZE         16
#define STATE_SYNC1     0
#define STATE_SYNC2     1
#define STATE_RXHDR     2
#define STATE_RXDATA    3

int rxstate = STATE_SYNC1;
unsigned int rxcount = 0;
uint8_t *rxptr = NULL;

static char *configName = (char *) "PICOLIGHT2";


#include "picoprotocol.h"

lsmessage_t message;
lsmessage_t txMessage;

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

/*  *********************************************************************
    *  Blinky LED
    ********************************************************************* */

static xtimer_t blinky_timer;
static xtimer_t displayUpdateTimer;
static char blinky_onoff = 0;

#define PIN_LED_PICOLIGHT 25
#define PIN_LED_PICOLASER 3


/*  *********************************************************************
    *  Global Variables
    ********************************************************************* */

//
// strip stack - specifies the order to run the animation renders
//

int8_t stripStack[MAXVSTRIPS];

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

// Maps the physical channel in the pstrip table to the pin# on the RP2040
const static uint8_t pinMapLED[16] = {
    PORT_A1, PORT_A2, PORT_A3, PORT_A4, PORT_A5, PORT_A6, PORT_A7, PORT_A8,
    PORT_B1, PORT_B2, PORT_B3, PORT_B4, PORT_B5, PORT_B6, PORT_B7, PORT_B8
};


//
// This array is for PICOLASER, and maps laser channels to their pins
//

#define PORT_LASER0     12
#define PORT_LASER1     13
#define PORT_LASER2     14
#define PORT_LASER3     15
#define PORT_LASER4     16
#define PORT_LASER5     17
#define PORT_LASER6     18
#define PORT_LASER7     19
#define PORT_LASER8     20
#define PORT_LASER9     21
#define PORT_STRIP0     10
#define PORT_STRIP1     11


// Maps the physical channel in the pstrip table to the pin# on the RP2040
const static uint8_t pinMapLaser[16] = {
    PORT_LASER0, PORT_LASER1, PORT_LASER2, PORT_LASER3, PORT_LASER4,
    PORT_LASER5, PORT_LASER6, PORT_LASER7, PORT_LASER8, PORT_LASER9,
    PORT_STRIP0, PORT_STRIP1,
    0, 0, 0, 0
};

//
// Physical strips, representing a string of neopixels attached to a given
// port and pin on the RP2040.
//

#define STRIPTYPE_LEDS_GRB  0
#define STRIPTYPE_LEDS_RGB  1
#define STRIPTYPE_LASER     2

#define LASER_OFF       0
#define LASER_ON        1

// This struct describes a physical strip
typedef struct PhysicalStrip_s {
    uint8_t pin;                        // Pin number for strips
    uint8_t type;                       // Type/flag info
    uint16_t length;                    // total # of pixels on strip
    Pico_NeoPixel *neopixels;           // Neopixel object.
} PhysicalStrip_t;

// Declare our global array of physical strips
PhysicalStrip_t physicalStrips[MAXPSTRIPS];


//
// OK, here are the "logical" strips, which can be composed from pieces of phyiscal strips.
// We leave the 32-bit encoding of the logical strip info from the protocol
// message, it's convient and compact.
//


typedef struct LogicalStrip_s {
    uint32_t substrips[MAXSUBSTRIPS];        // Encoded subset of pixels
    AlaLedRgb *alaStrip;                     // ALA object we created.
} LogicalStrip_t;


int logicalStripCount = 0;
LogicalStrip_t logicalStrips[MAXVSTRIPS];



/*  *********************************************************************
    *  Palettes
    ********************************************************************* */


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

#define GPIO25_PIN      (25u)
#define GPIO29_PIN      (29u)

bool PicoIsW = false;

// ad-hoc way to determine if is Pico or is Pico W
static void CheckPicoBoard(void)
{
    enum gpio_function pin25_func;
    enum gpio_function pin29_func;
    uint pin25_dir;
    uint pin29_dir;

    // remember pin directions
    pin25_dir = gpio_get_dir(GPIO25_PIN);
    pin29_dir = gpio_get_dir(GPIO29_PIN);

    // remember pin functions
    pin25_func = gpio_get_function(GPIO25_PIN);
    pin29_func = gpio_get_function(GPIO25_PIN);

    // activate pins
    gpio_init(GPIO25_PIN);
    gpio_init(GPIO29_PIN);

    // activate VSYS input
    gpio_set_dir(GPIO25_PIN, GPIO_OUT);
    gpio_put(GPIO25_PIN, 1);

    gpio_set_dir(GPIO29_PIN, GPIO_IN);
    if (gpio_get(GPIO29_PIN))   // should be high (sort-of, read ADC value better) on both Pico and Pico W
    {
        gpio_put(GPIO25_PIN, 0);        // disable VSYS input on Pico W
        if (!gpio_get(GPIO29_PIN))      // should be low from pull-down on Pico W
        {
            PicoIsW = true;             // is Pico W
        }
        else
        {
            PicoIsW = false;            // is Pico
        }
    }

    // restore pin functions
    gpio_set_function(GPIO25_PIN, pin25_func);
    gpio_set_function(GPIO29_PIN, pin29_func);

    // restore pin directions
    gpio_set_dir(GPIO25_PIN, pin25_dir);
    gpio_set_dir(GPIO29_PIN, pin29_dir);
}

void pushStrip(int strip)
{
    int i;
    
    // First remove the strip from the stack

    for (i = 0; i < MAXVSTRIPS; i++) {
        if (stripStack[i] == strip) {
            break;
        }
    }

    if (i != MAXVSTRIPS) {
        // Move everyone else up
        for (; i < MAXVSTRIPS-1; i++) {
            stripStack[i] = stripStack[i+1];
        }

        // Since we removed one, put a -1 at the end in its place.
        stripStack[MAXVSTRIPS-1] = -1;
    }

    // OK, now insert the new one at the top of the stack
    for (i = MAXVSTRIPS-1; i > 0; i--) {
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

void setAnimation(uint32_t *strips, int animation, int speed, unsigned int direction,
                  unsigned int option, 
                  AlaPalette palette, AlaColor color)
{
    int i;

    for (i = 0; i < logicalStripCount; i++) {
        // the "1 << i" means "shift 1 by 'i' positions to the left.
        // So, if i has the value 3, 1<<3 will mean the value 00001000 (binary)
        // Next, the "&" operator is a bitwise AND - for each bit
        // in "strips" we will AND that bit with the correspondig bit in (1<<i).
        // allowing us to test (check) if that particular bit is set.
        if ((logicalStrips[i].alaStrip != NULL) &&
            ((strips[i/32] & ((uint32_t)1 << (i & 31))) != 0)) {
            logicalStrips[i].alaStrip->forceAnimation(animation, speed, direction, option, palette, color);
            pushStrip(i);
        }
    }
}

/*  *********************************************************************
    *  reset_all()
    *  
    *  Reset all of the physical and logical strip tables and
    *  go back to the INIT state.
    *  
    *  Input parameters: 
    *      nothing
    *      
    *  Return value:
    *      nothing
    ********************************************************************* */

static void reset_all(void)
{
    int i;

    if (globalState == GSTATE_INIT) {
        return;
    }
           
    // Erase all the logical strips

    for (i = 0; i < MAXVSTRIPS; i++) {
        if (logicalStrips[i].alaStrip) {
            delete logicalStrips[i].alaStrip;
            logicalStrips[i].alaStrip = NULL;
            for (int j = 0; j < MAXSUBSTRIPS; j++) logicalStrips[i].substrips[j] = 0;
        }
    }

    logicalStripCount = 0;

    // Now erase the physical strips

    for (i = 0; i < MAXPSTRIPS; i++) {
        if (physicalStrips[i].neopixels) {
            delete physicalStrips[i].neopixels;
            physicalStrips[i].neopixels = NULL;
            physicalStrips[i].length = 0;
            physicalStrips[i].type = STRIPTYPE_LEDS_GRB;
            physicalStrips[i].pin = 0;
        }
    }

    globalState = GSTATE_INIT;
}


static void init_all(void)
{
    int i;
    int ss;

    if (globalState == GSTATE_READY) {
        return;
    }

    displayInit("STARTING");
    displayUpdate();

    // Walk the physical strip table as uploaded from lightscript and
    // instantiate the real strips.
    for (i = 0; i < MAXPSTRIPS; i++) {
        PhysicalStrip_t *pstrip = &physicalStrips[i];
        switch (pstrip->type) {
            // Initialize a neopixel strip
            case STRIPTYPE_LEDS_GRB:
            case STRIPTYPE_LEDS_RGB:
                if (pstrip->length != 0) {
                    pstrip->neopixels =
                        new Pico_NeoPixel(&wsp, pstrip->pin, pstrip->length, NEO_GRB);
                    if (!pstrip->neopixels) {
                        displayInit("ERR1");
                        displayUpdate();
                        printf("Out of memory creating physcial strips\n");
                    }
                    pstrip->neopixels->begin();
                }
                break;
            // initialize a laser
            case STRIPTYPE_LASER:
                gpio_init(pstrip->pin);
                gpio_set_dir(pstrip->pin, GPIO_OUT);
                gpio_put(pstrip->pin, LASER_OFF);
                break;
        }
    }

    // Now init the virtual strips.
    for (i = 0; i < MAXVSTRIPS; i++) {
        // Zero is not a valid encoded substrip, especially for the first substrip
        // in a logical strip (this is because its length would be zero).  If we
        // see this, then there's no strip here to init.
        if (logicalStrips[i].substrips[0] == 0) {
            continue;
        }
        // Create a new logical strip
        AlaLedRgb *alaLed = new AlaLedRgb;
        if (!alaLed) {
            displayInit("ERR2");
            displayUpdate();
            printf("Out of memory creating logical strips\n");
        }

        for (ss = 0; ss < MAXSUBSTRIPS; ss++) {
            // Get the encoded value of the substrip
            uint32_t substrip = logicalStrips[i].substrips[ss];
            uint32_t start = SUBSTRIP_START(substrip);
            uint32_t count = SUBSTRIP_COUNT(substrip);
            uint32_t reverse = SUBSTRIP_DIRECTION(substrip);
            uint32_t chan = SUBSTRIP_CHAN(substrip);

            // Just in case we blow it and pass in zero.
            if (count != 0) {
                alaLed->addSubStrip(start,
                                    count,
                                    reverse,
                                    physicalStrips[chan].neopixels);
            }

            // Bail if the EOT flag is set.
            if (SUBSTRIP_ISEOT(substrip)) {
                break;
            }
            substrip++;
        }
        logicalStrips[i].alaStrip = alaLed;
    }

    // OK, go back and "begin" all of the strips.  This allocates
    // the underlying memory for the pixels.
    for (i = 0; i < MAXVSTRIPS; i++) {
        if (logicalStrips[i].alaStrip) {
            logicalStrips[i].alaStrip->begin();
        } else {
            break;
        }
    }

    // When we get here, 'i' will be our total number of logical strips.
    // In our main loop we don't necessarily want to walk all 128 table
    // entries if we've only defined a few strips.
    logicalStripCount = i;

    // Init the strip stack
    for (i = 0; i < MAXVSTRIPS; i++) {
        stripStack[i] = -1;
    }

    displayInit("READY");
    displayUpdate();
    // Ready for action
    globalState = GSTATE_READY;
}


/*  *********************************************************************
    *  setup()
    *  
    *  Arduino SETUP routine - perform once-only initialization.
    ********************************************************************* */

static int pin_led = PIN_LED_PICOLIGHT;

void setup()
{
    //
    // Determine what type of RPI Pico board we are (Sets PicoIsW global)
    //

    CheckPicoBoard();

    pin_led = PicoIsW ? PIN_LED_PICOLASER : PIN_LED_PICOLIGHT;
    configName = (char *) (PicoIsW ? "PICOLASER" : "PICOLIGHT2");
    
    //
    // Set up our "timer", which lets us check to see how much time
    // has passed.  We might not use it for much, but it is handy to have.
    // For now, we'll mostly use it to blink the LED on the Arduino
    //

    TIMER_UPDATE();                 // remember current time
    TIMER_SET(blinky_timer,500);    // set timer for first blink

    gpio_init(pin_led);
    gpio_set_dir(pin_led, GPIO_OUT);

    // testing testing
    gpio_init(PORT_LASER0);
    gpio_set_dir(PORT_LASER0, GPIO_OUT);

    // Set up the Pico's programmable IO pins.

    ws2812_program_init(&wsp, pio0, 0, 800000);

    // Probably don't need to call reset_all() at power-on but...

    reset_all();
    
    // Initialize all of the physical strips

    // By default, run the "idle white" animation on all strips.
    // We can change this later if we want the art exhibit to start quietly.
    //setAnimation(ALLSTRIPS, ALA_IDLEWHITE, 1000, 0, 0, alaPalRgb, 0);


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
        gpio_put(pin_led, blinky_onoff);
        gpio_put(PORT_LASER0, blinky_onoff);
        if (globalState == GSTATE_INIT) {
            TIMER_SET(blinky_timer,500);
        } else {
            TIMER_SET(blinky_timer,250);
        }
    }
}

static void sendMessage(lsmessage_t *msg)
{
    uint8_t *ptr;
    int len;
    
    putchar(0x02);
    putchar(0xAA);

    ptr = (uint8_t *) msg;
    len = LSMSG_HDRSIZE + (int) msg->ls_length;
    while (len) {
        putchar(*ptr);
        ptr++;
        len--;
    }
}


static void sendStatusMessage(uint32_t status)
{
    txMessage.ls_command = LSCMD_STATUS;
    txMessage.ls_length = sizeof(lsstatus_t);
    txMessage.info.ls_status.ls_status = status;

    sendMessage(&txMessage);
}



static void handleAnimationMessage(lsmessage_t *msg)
{
    int animation, speed;
    uint32_t palette;
    unsigned int direction;
    unsigned int option;
    AlaPalette ap;
    AlaColor color = 0;
    lsanimate_t *amsg = &(msg->info.ls_animate);

    // 
    TIMER_CLEAR(displayUpdateTimer);

    animation = amsg->la_anim;
    speed     = amsg->la_speed;
    // Use the top bit fo the animation to indicate the direction.
    direction = (animation & 0x8000) ? 1 : 0;
    animation = (animation & 0x7FFF);
    palette   = amsg->la_color;
    option    = amsg->la_option;

    // Use bit 24 (after the 3 color values) to indicate the palette vs color.
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

    setAnimation(&(amsg->la_strips[0]), animation, speed, direction, option, ap, color);

    // No response is sent for this one.
    
}

static void handleBrightnessMessage(lsmessage_t *msg)
{
    // No response is sent for this one.
}

static void handleIdleMessage(lsmessage_t *msg)
{
    // No response is sent for this one.
}
                                    

                                    
static void handleVersionMessage(lsmessage_t *msg)
{
    txMessage.ls_command = LSCMD_VERSION;
    txMessage.ls_length = sizeof(lsversion_t);
    txMessage.info.ls_version.lv_protocol = 2;
    txMessage.info.ls_version.lv_major = 2;
    txMessage.info.ls_version.lv_minor = 0;
    txMessage.info.ls_version.lv_eco = 0;
    sendMessage(&txMessage);
}
                                    
static void handleStatusMessage(lsmessage_t *msg)
{
    sendStatusMessage(0);
}
                                    
static void handleResetMessage(lsmessage_t *msg)
{
    reset_all();
    sendStatusMessage(0);
}

static void handleSetVStripMessage(lsmessage_t *msg)
{
    lsvstrip_t *vstrip = &(msg->info.ls_vstrip);
    uint32_t count = vstrip->lv_count;

    if (vstrip->lv_idx > MAXVSTRIPS) {
        sendStatusMessage(0xFFFFFFFF);
        return;
    }

    if (count > MAXSUBSTRIPS) count = MAXSUBSTRIPS;
    if (count == 0) {
        sendStatusMessage(0xFFFFFFFF);
        return;
    }

    memcpy(&(logicalStrips[vstrip->lv_idx].substrips), vstrip->lv_substrips, count*sizeof(uint32_t));

    // Terminate the strip list if it is not already.
    logicalStrips[vstrip->lv_idx].substrips[count-1] |= ENCODESUBSTRIP(0,0,0,SUBSTRIP_EOT);
    
    sendStatusMessage(0);
}

static void handleSetPStripMessage(lsmessage_t *msg)
{
    uint32_t info = msg->info.ls_pstrip.lp_pstrip;
    uint32_t chan = PSTRIP_CHAN(info);
    uint32_t type = PSTRIP_TYPE(info);
    uint32_t count = PSTRIP_COUNT(info);

    physicalStrips[chan].type = STRIPTYPE_LEDS_GRB;
    physicalStrips[chan].pin = pinMapLED[chan];
    physicalStrips[chan].type = type;
    physicalStrips[chan].length = count;

    sendStatusMessage(0);
}

static void handleInitMessage(lsmessage_t *msg)
{
    init_all();
    sendStatusMessage(0);
}
                                    


static void handleMessage(lsmessage_t *msg)
{
    switch (msg->ls_command) {
        case LSCMD_ANIMATE:
            handleAnimationMessage(msg);
            break;
        case LSCMD_BRIGHTNESS:
            handleBrightnessMessage(msg);
            break;
        case LSCMD_IDLE:
            handleIdleMessage(msg);
            break;
        case LSCMD_VERSION:
            handleVersionMessage(msg);
            break;
        case LSCMD_STATUS:
            handleStatusMessage(msg);
            break;
        case LSCMD_RESET:
            handleResetMessage(msg);
            break;
        case LSCMD_SETPSTRIP:
            handleSetPStripMessage(msg);
            break;
        case LSCMD_SETVSTRIP:
            handleSetVStripMessage(msg);
            break;
        case LSCMD_INIT:
            handleInitMessage(msg);
            break;
        default:
            break;
    }

}

void checkForMessage(void)
{
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
                if (b == 0x02) {
                    rxstate = STATE_SYNC2;
                }
                break;
            case STATE_SYNC2:
                if (b == 0xAA) {
                    rxptr = (uint8_t *) &message;
                    rxcount = LSMSG_HDRSIZE;
                    rxstate = STATE_RXHDR;
                }
                else {
                    rxstate = STATE_SYNC1;
                }
                break;
            case STATE_RXHDR:
                *rxptr++ = b;
                rxcount--;
                if (rxcount == 0) {
                    rxstate = STATE_RXDATA;
                    rxcount = message.ls_length;
                    // Sanity check, should not be needed.
                    if (rxcount > sizeof(message.info)) {
                        rxcount = sizeof(message.info);
                        }
                    if (rxcount == 0) {
                        // Handle the case of no payload
                        handleMessage(&message);
                        rxstate = STATE_SYNC1;
                    }
                }
                break;
            case STATE_RXDATA:
                *rxptr++ = b;
                rxcount--;
                if (rxcount == 0) {
                    rxstate = STATE_SYNC1;
                    handleMessage(&message);
                }
                break;
        }
    }
}

void first_time_idle(void)
{
    int i;
    // create a single physical strip of 256 LEDs.
    Pico_NeoPixel *pixels = new Pico_NeoPixel(&wsp, PORT_A1, 256, NEO_GRB);

    pixels->begin();
    
    for (i = 0; i < 256; i++) {
        // Same color as IDLEWHITE
        pixels->setPixelColor((uint16_t)i, 10, 10, 10);
    }

    for (i = 0; i < MAXPSTRIPS; i++) {
        pixels->setPin(pinMapLED[i]);
        pixels->show();
    }

    delete pixels;
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
        // The PicoLaser always powers the display so we don't need to refresh it.
        if (!PicoIsW) {
            displayInit(configName);
            displayUpdate();
        }
        TIMER_SET(displayUpdateTimer, 5000);
    }

    //
    // Run animations on all strips
    //

    if (globalState == GSTATE_READY) {
        // First compute new pixels on the LOGICAL Strips
#if 0
        for (i = logicalStripCount-1; i >= 0; i--) {
            if (stripStack[i] != -1) {
                logicalStrips[stripStack[i]].alaStrip->runAnimation();
            }
        }
#else
        for (i = 0; i < logicalStripCount; i++) {
            logicalStrips[i].alaStrip->runAnimation();
        }
#endif

        // Now send the data to the PHYSICAL strips
        for (i = 0; i < MAXPSTRIPS; i++) {
            if (physicalStrips[i].neopixels != NULL) {
                physicalStrips[i].neopixels->show();
            }
        }
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

    setup();

    displayInit(configName);
    displayInit(configName);
    displayUpdate();

    // PicoLaser doesn't yet have an idle startup.
    if (!PicoIsW) {
        first_time_idle();
    }

    TIMER_SET(displayUpdateTimer, 5000);

    for (;;) {
        loop();
    }
}
