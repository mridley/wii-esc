/**
 * Wii-ESC NG 1.0 - 2013
 * Main program file.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <pt.h>
#include <pt-sem.h>
#include "global.h"
#include "config.h"
#include "core.h"
#include "rx.h"
#include "power_stage.h"

void setup_to_rt() {
  pwr_stage.braking_enabled = 0;
  if (cfg.braking) pwr_stage.braking_enabled = 1;
  timing_adv = cfg.timing_adv;
  rx_setup_rt();
  pwr_stage.rev = 0;
  if (cfg.rev) pwr_stage.rev = 1;
}

void beep(uint8_t khz, uint8_t len) {
 uint16_t off = 10000 / khz;
 uint16_t cnt = khz * len;
 for (uint16_t i = 0; i < cnt; i++) {
   set_pwm_on(pwr_stage.com_state);
   __delay_us(6);
   set_pwm_off(pwr_stage.com_state);
   __delay_us(off);
 }
}

void startup_sound() {
  pwr_stage.com_state = 0; set_comm_state();
  for (uint8_t i = 0; i < 5; i++) {
    pwr_stage.com_state = i;
    change_comm_state(i);
    beep(6 + i, 10);
    __delay_ms(5);
  }
}

void wait_for(uint16_t low, uint16_t high, uint8_t cnt) {
  uint8_t _cnt = cnt;
  while (1) {
    uint16_t tmp = rx_get_frame();
    if ((tmp >= low) && (tmp <= high)) {
      if (!(--_cnt)) break;
    } else _cnt = cnt;
  }
}

uint16_t get_stable_ppm_value() {
  uint16_t tmp = rx_get_frame();
  uint16_t frame_min = tmp;
  uint16_t frame_max = tmp;
  for (uint8_t i = 0; i < 50; i++) {
    tmp = rx_get_frame();
    if (tmp > frame_max) frame_max = tmp;
    if (tmp < frame_min) frame_min = tmp;
    if ((frame_max - frame_min) > US_TO_TICKS(4)) return 0;
  }
  return (frame_max + frame_min) >> 1;
}

#define RC_DEAD   50
#define RC_CENTRE 1500

// centre
void wait_for_center() {
  wait_for(US_TO_TICKS(RC_CENTRE - RC_DEAD), US_TO_TICKS(RC_CENTRE + RC_DEAD), 15);
}

void calibrate_osc() {
#if defined(RCP_CAL) && defined(INT_OSC)
  if (rx.rcp_cal == 0) return;
  while (rx_get_frame() > rx.rcp_cal) {
    uint8_t tmp = OSCCAL;
    if (!(--tmp)) break;
    OSCCAL = tmp;
    rx_get_frame();
  }
  while (rx_get_frame() < rx.rcp_cal) {
    uint8_t tmp = OSCCAL;
    if ((++tmp) == 0) break;
    OSCCAL = tmp;
    rx_get_frame();
  }
#endif
}

//arduino/main.cpp calls this
void setup() {
  cli();
  Board_Init();
  PowerStage_Init();
  RX_Init();
  sei();
  setup_to_rt();
  __delay_ms(250);
  startup_sound();
  calibrate_osc();
}

#define STATE_PWR 0x10
#define STATE_DIR 0x20
#define STATE_SEQ 0x03 

void square_wave(uint8_t& state)
{
  if (!(state & STATE_PWR))
  {
    return;
  }
  uint8_t seq = state & STATE_SEQ; 
  
  if (state & STATE_DIR)
  {
    CnFETOff();
    CpFETOff();
    switch(seq)
    {
    case 0:
      AnFETOff();
      BpFETOff();
      ApFETOn();
      BnFETOn();
      break;
    case 2:
      ApFETOff();
      BnFETOff();
      AnFETOn();
      BpFETOn();
      break;
    default:
      free_spin();
      break;
    }  
  }
  else
  {
    AnFETOff();
    ApFETOff();
    switch(seq)
    {
    case 0:
      CnFETOff();
      BpFETOff();
      CpFETOn();
      BnFETOn();
      break;
    case 2:
      CpFETOff();
      BnFETOff();
      CnFETOn();
      BpFETOn();
      break;
    default:
      free_spin();
      break;
    }
  }
  
  // quarter period of 60Hz (4.166 ms)
  __delay_us(4166);

  state += 1;
  state &= 0xF3;
}

//arduino/main.cpp calls this
void loop() 
{
  set_comm_state(); 
  for (;;) 
  {
    free_spin();
    wait_for_center();
    beep(12, 50);
    free_spin();
    
    uint8_t state = 0;

    for(;;)
    {
      filter_ppm_data();
      if (rx.raw > US_TO_TICKS(RC_CENTRE + RC_DEAD))
      {
        state |= STATE_PWR;
        state |= STATE_DIR;
        square_wave(state);
      }
      else if (rx.raw < US_TO_TICKS(RC_CENTRE - RC_DEAD))
      {
        state |= STATE_PWR;
        state &= (~STATE_DIR);
        square_wave(state);
      }
      else
      {
        if (state & STATE_PWR)
        { 
          free_spin();
          brake();        
          state &= (~STATE_PWR);
        }
        __delay_ms(8);
      }
    }
  }
}

