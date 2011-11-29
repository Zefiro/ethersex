#ifndef SHIFTBRITE_H
#define SHIFTBRITE_H

#define TIMER_HZ (1000/20/5)

typedef unsigned int word;
typedef unsigned char byte;

union tMoodlightData { 
    struct {
        int nr;
        int ng;
        int nb;
    } singleColor;
    struct {
        double hue;
        double hue_delta;
    } hsv_shift;
    struct {
        byte state;
        int dimm;
    } linear;
    struct {
        byte mode;
        byte state;
        int step_countdown;
        int step_size;
    } pulsar;
}; 

struct tMoodlightFunction {
    byte mode;
    union tMoodlightData data;
};

extern struct tMoodlightFunction MoodlightData[CONF_SHIFTBRITE_PASS_COUNT][CONF_SHIFTBRITE_LEDCOUNT];
extern int LEDChannels[CONF_SHIFTBRITE_LEDCOUNT][3];
extern byte shiftbrite_enabled;

#ifdef DEBUG_SHIFTBRITE
#include "core/debug.h"
#define SB_DEBUG(a...)  debug_printf("shiftbrite: " a)
#else
#define SB_DEBUG(a...)
#endif


void shiftbrite_timer(void);


#endif