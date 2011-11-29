/*
 *
 * Copyright (c) 2007 by Christian Dietrich <stettberger@dokucode.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * For more information on the GPL, please go to:
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <avr/interrupt.h>

#include "core/debug.h"
#include "config.h"
#include "shiftbrite.h"
#include <util/delay.h>

int LEDChannels[CONF_SHIFTBRITE_LEDCOUNT][3];
struct tMoodlightFunction MoodlightData[CONF_SHIFTBRITE_PASS_COUNT][CONF_SHIFTBRITE_LEDCOUNT];
byte shiftbrite_enabled;

void SB_SendBits(int data) {
    for(byte num = 8;num > 0; num--) {
        if (data & 0x80) { 
            PIN_CLEAR(SHIFTBRITE_CLOCK);
            PIN_SET(SHIFTBRITE_DATA); 
        } else {
            PIN_CLEAR(SHIFTBRITE_CLOCK);
            PIN_CLEAR(SHIFTBRITE_DATA);
        }
        _delay_us(1);
        PIN_SET(SHIFTBRITE_CLOCK);
        _delay_us(1);
        data = data << 1;
    }
    PIN_CLEAR(SHIFTBRITE_CLOCK);
}

void SB_SendPacket(byte SB_CommandMode, int SB_RedCommand, int SB_GreenCommand, int SB_BlueCommand) {
    cli();
    SB_SendBits(SB_CommandMode << 6 | SB_BlueCommand>>4);
    SB_SendBits(SB_BlueCommand<<4 | SB_RedCommand>>6);
    SB_SendBits(SB_RedCommand << 2 | SB_GreenCommand>>8);
    SB_SendBits(SB_GreenCommand);
    sei();
}
 
void output_shift()
{
    // invert idx, to have idx=0 be the one nearest to the AVR (aka last written to)
    for (int idx = CONF_SHIFTBRITE_LEDCOUNT-1; idx >= 0; idx--) {
	  SB_SendPacket(0, LEDChannels[idx][0], LEDChannels[idx][1], LEDChannels[idx][2]);
    }
 
    _delay_us(15);
    PIN_SET(SHIFTBRITE_LATCH);
    _delay_us(15);
    PIN_CLEAR(SHIFTBRITE_LATCH);
 
    for (int idx = 0; idx < CONF_SHIFTBRITE_LEDCOUNT; idx++) {
      SB_SendPacket(1, 120, 100, 100);
    }
    _delay_us(15);
    PIN_SET(SHIFTBRITE_LATCH);
    _delay_us(15);
    PIN_CLEAR(SHIFTBRITE_LATCH);
}

// adapted from http://www.programmersheaven.com/download/15218/download.aspx
// calculated rgb values of 0-1023
int nr, ng, nb;
#include <math.h>

// input values, h = 0 - 240 (circular, 0=240), s,v = 0.0 - 1.0
void convert_hsv_to_rgb(double hue, double s, double v) {
    double p1, p2, p3, i, f;
    int pv;
    double xh;
    // convert hue to be in 0-6
    xh = hue / 40.0;
    // i = greatest integer <= h
    i = (double)floor((double)xh);
    // f = fractional part of h
    f = xh - i;
    p1 = v * (1 - s);
    p2 = v * (1 - (s * f));
    p3 = v * (1 - (s * (1 - f)));

    p1 = (long)(p1 * 1023.0);
    p2 = (long)(p2 * 1023.0);
    p3 = (long)(p3 * 1023.0);
    pv = (long)(v * 1023.0);

    switch ((int) i) {
        case 0:
            nr = pv;
			ng = p3;
			nb = p1;
		    break;
        case 1:
			nr = p2;
			ng = pv;
			nb = p1;
			break;
        case 2:
			nr = p1;
			ng = pv;
			nb = p3;
			break;
        case 3:
			nr = p1;
			ng = p2;
			nb = pv;
			break;
        case 4:
			nr = p3;
			ng = p1;
			nb = pv;
			break;
        case 5:
			nr = pv;
			ng = p1;
			nb = p2;
			break;
    }

}

void getColor(byte pass, byte idx) {
    union tMoodlightData *data = &(MoodlightData[pass][idx].data);
    switch(MoodlightData[pass][idx].mode) {
        case 1:
            nr = data->singleColor.nr;
            ng = data->singleColor.ng;
            nb = data->singleColor.nb;
            break;
        case 2:
            data->hsv_shift.hue += data->hsv_shift.hue_delta;
            while (data->hsv_shift.hue >= 240) { data->hsv_shift.hue -= 240; }
            while (data->hsv_shift.hue < 0) data->hsv_shift.hue += 240;
            convert_hsv_to_rgb(data->hsv_shift.hue, 1.0, 1.0);
            break;
        case 3:
            switch (data->linear.state) {
                case  0: nr =                        0; ng =        data->linear.dimm; nb =                        0; break;
                case  1: nr =                        0; ng = 1023                    ; nb =                        0; break;
                case  2: nr =        data->linear.dimm; ng = 1023                    ; nb =                        0; break;
                case  3: nr = 1023                    ; ng = 1023                    ; nb =                        0; break;
                case  4: nr = 1023                    ; ng = 1023 - data->linear.dimm; nb =                        0; break;
                case  5: nr = 1023                    ; ng =                        0; nb =                        0; break;
                case  6: nr = 1023                    ; ng =                        0; nb =        data->linear.dimm; break;
                case  7: nr = 1023                    ; ng =                        0; nb = 1023                    ; break;
                case  8: nr = 1023 - data->linear.dimm; ng =                        0; nb = 1023                    ; break;
                case  9: nr =                        0; ng =                        0; nb = 1023                    ; break;
                case 10: nr =                        0; ng =        data->linear.dimm; nb = 1023                    ; break;
                case 11: nr =                        0; ng = 1023                    ; nb = 1023                    ; break;
                case 12: nr =                        0; ng = 1023                    ; nb = 1023 - data->linear.dimm; break;
                default: nr = ng = nb = 0;
            }
            data->linear.dimm++;
            if (data->linear.dimm > 1023) {
                data->linear.dimm = 0;
                data->linear.state++;
                if (data->linear.state > 12) {
                    data->linear.state = 1;
                }
            }
            break;
        case 4:
            switch (data->pulsar.mode) {
                case 0:
                    switch (data->pulsar.state) {
                        default:
                            data->pulsar.state = 0;
                        case 0:
                            nr = 1023;
                            ng = nb = 0;
                            break;                        
                        case 1:
                            break;
                    }
                break;
            }
            if (data->pulsar.step_countdown == 0) {
                data->pulsar.step_countdown = data->pulsar.step_size-1;
                data->pulsar.state++;
            } else data->pulsar.step_countdown--;
            break;
        case 0:
        default:
            nr = ng = nb = 0;
    }
}

void calculate_colors() {
    for(int idx = 0; idx < CONF_SHIFTBRITE_LEDCOUNT; idx++) {
        for(byte pass = 0; pass < CONF_SHIFTBRITE_PASS_COUNT; pass++) {
            if (pass == 0 || MoodlightData[pass][idx].mode != 0) {
                getColor(pass, idx);
            }
        }
        LEDChannels[idx][0] = nr;
        LEDChannels[idx][1] = ng;
        LEDChannels[idx][2] = nb;
    } 
}

void shiftbrite_timer(void)
{
    if (shiftbrite_enabled) {
        calculate_colors();
        output_shift();
        PIN_CLEAR(SHIFTBRITE_ENABLE); // active low
    } else {
        PIN_SET(SHIFTBRITE_ENABLE); // active low
    }
}

void shiftbrite_init(void)
{
    SB_DEBUG("Shiftbrite started\n");
    shiftbrite_enabled = 1;
}

/*
  -- Ethersex META --
  header(hardware/shiftbrite/shiftbrite.h)
  init(shiftbrite_init)
  timer(5, shiftbrite_timer())
*/
