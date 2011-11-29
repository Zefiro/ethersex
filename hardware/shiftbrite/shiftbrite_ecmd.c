/*
 *
 * Copyright (c) by Alexander Neumann <alexander@bumpern.de>
 * Copyright (c) 2007 by Stefan Siegl <stesie@brokenpipe.de>
 * Copyright (c) 2007 by Christian Dietrich <stettberger@dokucode.de>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License (either version 2 or
 * version 3) as published by the Free Software Foundation.
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

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#include <string.h>
#include <avr/eeprom.h>

#include "config.h"
#include "core/debug.h"
#include "protocols/ecmd/ecmd-base.h"

#include "shiftbrite.h"

int16_t parse_cmd_shiftbrite (char *cmd, char *output, uint16_t len)
{
    SB_DEBUG("got '%s' (at %x)\n", cmd, cmd);
    while(*cmd == ' ' || *cmd == '\t') cmd++;
    if (*cmd == '\0') return ECMD_FINAL(snprintf_P(output, len, PSTR("Parse Error")));
    byte first_idx = 0;
    byte last_idx = CONF_SHIFTBRITE_LEDCOUNT-1;
    if (strncmp_P(cmd, PSTR("off"), 3) == 0) {
        shiftbrite_enabled = 0;
        SB_DEBUG("Shiftbrite stopped\n");
        return ECMD_FINAL(snprintf_P(output, len, PSTR("I'm gone, Master.")));
    }

    if (strncmp_P(cmd, PSTR("all"), 3) == 0) {
        cmd += 3;
    } else {
        first_idx = strtol(cmd, &cmd, 0);
        if (*cmd == '-') {
            cmd++;        
            last_idx = strtol(cmd, &cmd, 0);
        } else last_idx = first_idx;
    }
    if (first_idx >= CONF_SHIFTBRITE_LEDCOUNT) first_idx = 0;
    if (last_idx >= CONF_SHIFTBRITE_LEDCOUNT) last_idx = CONF_SHIFTBRITE_LEDCOUNT-1;
    
    while(*cmd == ' ' || *cmd == '\t') cmd++;
    if (*cmd == '\0') return ECMD_FINAL(snprintf_P(output, len, PSTR("Parse Error")));

    byte pass = 0;
    if (strncmp_P(cmd, PSTR("p="), 2) == 0) {
        cmd += 2;
        pass = strtol(cmd, &cmd, 0);
        while(*cmd == ' ' || *cmd == '\t') cmd++;
        if (*cmd == '\0') return ECMD_FINAL(snprintf_P(output, len, PSTR("Parse Error")));
    }

    byte mode;
    char *command = cmd;
//    cmd = strtok(cmd, " ");
// strtok seems not to work - quick work-around
    while (*cmd != ' ' && *cmd != 0) { cmd++; }
    if (*cmd != 0) { *cmd = 0; cmd++; } else cmd = NULL;

    SB_DEBUG("%X %X\n", command, cmd);
    
    // first pass: get additional parameters
    double delay, speed;
    int nr, ng, nb;
    switch(command[0]) {
        case 'c':
            if (cmd == NULL) return ECMD_FINAL(snprintf_P(output, len, PSTR("Parse Error")));
            nr = strtol(cmd, &cmd, 0);
            ng = strtol(cmd, &cmd, 0);
            nb = strtol(cmd, &cmd, 0);
            SB_DEBUG("c: %d %d %d\n", nr, ng, nb);
            break;
        case 'm':
            if (cmd != NULL) {
                // amount of 1/10th seconds for a full hue cycle (delta is added each timer = 20ms)
                speed = 240.0 / TIMER_HZ / 10 / strtol(cmd, &cmd, 0);
                // amount of delay between LEDs in 1/10th seconds
                delay = speed * TIMER_HZ / 10 * strtol(cmd, &cmd, 0);
            } else {
                speed = 1;
                delay = 0;
            SB_DEBUG("mood default: d=%f, id=%f\n", speed, delay);
            }
            SB_DEBUG("mood: d=%d, id=%d\n", floor(speed), floor(delay));
            break;
    }

    // second pass: set for all LEDs    
    for (byte idx = first_idx; idx <= last_idx; idx++) {
        MoodlightData[pass][idx].mode = 0; // temporarily black out until mode data is updated completely
        union tMoodlightData *data = &(MoodlightData[pass][idx].data);
        switch(command[0]) {
            case 'c':
                mode = 1;
                data->singleColor.nr = nr;
                data->singleColor.ng = ng;
                data->singleColor.nb = nb;
                break;
            case 'r':
                mode = 1;
                data->singleColor.nr = 1023;
                data->singleColor.ng = 0;
                data->singleColor.nb = 0;
                break;
            case 'g':
                mode = 1;
                data->singleColor.nr = 0;
                data->singleColor.ng = 1023;
                data->singleColor.nb = 0;
                break;
            case 'b':
                mode = 1;
                data->singleColor.nr = 0;
                data->singleColor.ng = 0;
                data->singleColor.nb = 1023;
                break;
            case 'm':
                mode = 2;
                data->hsv_shift.hue_delta = speed;
                data->hsv_shift.hue = idx*delay;
                break;
            case 'l':
                mode = 3;
                data->linear.state = -1; // start with black
                data->linear.dimm = 1023-idx*16*256/4*TIMER_HZ/1000; // should increase at a rate of 4 per 250ms (4/(250/(1000/20))), start with 4s (=16*256/4/20 per timer) difference between each led
                break;
            case 'p':
                mode = 4;
                data->pulsar.step_countdown = 0;
                data->pulsar.step_size = TIMER_HZ;
                data->pulsar.mode = 0;
                break;
            case 'o':
                mode = 0;
                break;
            default:
                mode = 0;
        }
        MoodlightData[pass][idx].mode = mode;
    }

    if (mode) {
        shiftbrite_enabled = 1;
        SB_DEBUG("Shiftbrite mode changed (mode=%i, p=%i, idx=%i-%i)\n", mode, pass, first_idx, last_idx);
    } else {
        // TODO check whether *ALL* LEDs are off now
        shiftbrite_enabled = 0;
        SB_DEBUG("Shiftbrite stopped\n");
    }

    return ECMD_FINAL(snprintf_P(output, len, PSTR("As you wish, my Dragon.")));
}

/*
  -- Ethersex META --
  block(ShiftBrite)
  ecmd_feature(shiftbrite, "shiftbrite",foo, Control the Shiftbrite)
*/
