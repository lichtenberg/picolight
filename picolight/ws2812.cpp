
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

#include "PicoNeoPixel.h"
#include "AlaLedRgb.h"

#define IS_RGBW false
#define NUM_PIXELS 16


#define WS2812_PIN 6


#if 0
static void fillStrip(Pico_NeoPixel *s, uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0; i < s->numPixels(); i++) {
        s->setPixelColor(i, r, g, b);
    }
}
#endif
    

int main() {
    //set_sys_clock_48();
    stdio_init_all();
    for (uint i = 0; i < 10; i++) {
        printf("%u... ", 9-i);
        sleep_ms(1000);
    }


    ws2812pio_t ws;

    ws2812_program_init(&ws, pio0, 0, 800000);

    Pico_NeoPixel *pnp1 = new Pico_NeoPixel(&ws,WS2812_PIN,16);
    pnp1->begin();
    Pico_NeoPixel *pnp2 = new Pico_NeoPixel(&ws,WS2812_PIN+1,16);
    pnp2->begin();

    AlaLedRgb *ala1, *ala2;

    printf("Creating ALAs...\n");
    
    ala1 = new AlaLedRgb();
    ala1->initSubStrip(0,16,pnp1);
    ala2 = new AlaLedRgb();
    ala2->initSubStrip(0,16,pnp2);

    printf("Setting animations...\n");
    ala1->setAnimation(ALA_MOVINGBARS, 1000, 0, 0, alaPalRgb, 0);
    ala2->setAnimation(ALA_MOVINGBARS, 1000, 0, 0, alaPalRgb, 0);

    printf("Running animations...\n");

    int c = 0;

    
    while (1) {
        ala1->runAnimation();
        ala2->runAnimation();
        pnp1->show();
        pnp2->show();
        c++;
        if (c > 10000) {
            c = 0;
            printf(".");
            }
        #if 0
        fillStrip(pnp1, 0xFF, 0x00, 0x00);
        fillStrip(pnp2, 0x00, 0xFF, 0x00);
        pnp1->show();
        pnp2->show();
        sleep_ms(500);

        fillStrip(pnp1, 0x00, 0xFF, 0x00);
        fillStrip(pnp2, 0x00, 0x00, 0xFF);
        pnp1->show();
        pnp2->show();
        sleep_ms(500);

        fillStrip(pnp1, 0x00, 0x00, 0xFF);
        fillStrip(pnp2, 0xFF, 0x00, 0x00);
        pnp1->show();
        pnp2->show();
        sleep_ms(500);
        #endif
    }
}
