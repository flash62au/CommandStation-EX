/*
 *  © 2021 Mike S
 *  © 2021 Harald Barth
 *  © 2021 Fred Decker
 *  © 2021 Chris Harlow
 *  © 2021 David Cutting
 *  © 2022 Paul M. Antoine
 *  All rights reserved.
 *  
 *  This file is part of Asbelos DCC API
 *
 *  This is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  It is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CommandStation.  If not, see <https://www.gnu.org/licenses/>.
 */

// ATTENTION: this file only compiles on a TEENSY
// Please refer to DCCTimer.h for general comments about how this class works
// This is to avoid repetition and duplication.
#ifdef ARDUINO_ARCH_SAMD

#include "FSH.h" //PMA temp debug
#include "DIAG.h" //PMA temp debug
#include "DCCTimer.h"

INTERRUPT_CALLBACK interruptHandler=0;

void DCCTimer::begin(INTERRUPT_CALLBACK callback) {
  interruptHandler=callback;
  noInterrupts();

  // PMA - Set up ADC to do faster reads... default for Arduino Zero platform configs is 436uS,
  // and we need sub-100uS. This code sets it to a read speed of around 21uS, and enables 12-bit
  ADC->CTRLA.bit.ENABLE = 0;              // disable ADC
  while( ADC->STATUS.bit.SYNCBUSY == 1 ); // wait for synchronization

  ADC->CTRLB.reg &= 0b1111100011111111;          // mask PRESCALER bits
  ADC->CTRLB.reg |= ADC_CTRLB_PRESCALER_DIV64 |  // divide Clock by 64
                    ADC_CTRLB_RESSEL_10BIT;      // Result on 10 bits default, 12 bits possible

  ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM_1 |   // take 1 sample at a time
                     ADC_AVGCTRL_ADJRES(0x00ul); // adjusting result by 0
  ADC->SAMPCTRL.reg = 0x00;                      // sampling Time Length = 0

  ADC->CTRLA.bit.ENABLE = 1;                     // enable ADC
  while(ADC->STATUS.bit.SYNCBUSY == 1);          // wait for synchronization

  // PMA - actual timer setup goo
  // Setup clock sources first
  REG_GCLK_GENDIV =   GCLK_GENDIV_DIV(1) |            // Divide 48MHz by 1
                      GCLK_GENDIV_ID(4);              // Apply to GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);                  // Wait for synchronization
                
  REG_GCLK_GENCTRL =  GCLK_GENCTRL_GENEN |            // Enable GCLK
                      GCLK_GENCTRL_SRC_DFLL48M |      // Set the 48MHz clock source
                      GCLK_GENCTRL_ID(4);             // Select GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);                  // Wait for synchronization
            
  REG_GCLK_CLKCTRL =  GCLK_CLKCTRL_CLKEN |            // Enable generic clock
                      4 << GCLK_CLKCTRL_GEN_Pos |     // Apply to GCLK4
                      GCLK_CLKCTRL_ID_TCC0_TCC1;      // Feed GCLK to TCC0/1
  while (GCLK->STATUS.bit.SYNCBUSY);

  // PMA - assume we're using TCC0
  REG_GCLK_GENDIV =   GCLK_GENDIV_DIV(1) |            // Divide 48MHz by 1
                      GCLK_GENDIV_ID(4);              // Apply to GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);                  // Wait for synchronization
                
  REG_GCLK_GENCTRL =  GCLK_GENCTRL_GENEN |            // Enable GCLK
                      GCLK_GENCTRL_SRC_DFLL48M |      // Set the 48MHz clock source
                      GCLK_GENCTRL_ID(4);             // Select GCLK4
  while (GCLK->STATUS.bit.SYNCBUSY);                  // Wait for synchronization

  REG_GCLK_CLKCTRL =  GCLK_CLKCTRL_CLKEN |            // Enable generic clock
                      4 << GCLK_CLKCTRL_GEN_Pos |     // Apply to GCLK4
                      GCLK_CLKCTRL_ID_TCC0_TCC1;      // Feed GCLK to TCC0/1
  while (GCLK->STATUS.bit.SYNCBUSY);                  // Wait for synchronization

  TCC0->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;     // Select NPWM as waveform
  while (TCC0->SYNCBUSY.bit.WAVE);            // Wait for sync
  TCC0->INTENSET.reg = TCC_INTENSET_OVF;      // Interrupt on overflow

  // PMA - set the frequency
  TCC0->CTRLA.reg |= TCC_CTRLA_PRESCALER(TCC_CTRLA_PRESCALER_DIV1_Val);
  unsigned long cycles = F_CPU / 10000000 * 58;   // 58uS to cycles
  unsigned long pwmPeriod = cycles / 2;
  TCC0->PER.reg = pwmPeriod;
  while (TCC0->SYNCBUSY.bit.PER);

  // PMA - start it
  TCC0->CTRLA.bit.ENABLE = 1;
  while (TCC0->SYNCBUSY.bit.ENABLE);

  NVIC_EnableIRQ((IRQn_Type)TCC0_IRQn);       // Enable the interrupt
  interrupts();
}

// PMA - IRQ handler copied from rf24 branch
// TODO: test
void TCC0_Handler() {
    if(TCC0->INTFLAG.bit.OVF) {
        TCC0->INTFLAG.bit.OVF = 1;
        interruptHandler();
    }
}


bool DCCTimer::isPWMPin(byte pin) {
       //TODO: SAMD digitalPinHasPWM
      (void) pin;
       return false;  // TODO what are the relevant pins? 
  }

void DCCTimer::setPWM(byte pin, bool high) {
    // TODO: what are the relevant pins?
    (void) pin;
    (void) high;
}

void   DCCTimer::getSimulatedMacAddress(byte mac[6]) {
  volatile uint32_t *serno1 = (volatile uint32_t *)0x0080A00C;
  volatile uint32_t *serno2 = (volatile uint32_t *)0x0080A040;
//  volatile uint32_t *serno3 = (volatile uint32_t *)0x0080A044;
//  volatile uint32_t *serno4 = (volatile uint32_t *)0x0080A048;

  volatile uint32_t m1 = *serno1;
  volatile uint32_t m2 = *serno2;
  mac[0] = m1 >> 8;
  mac[1] = m1 >> 0;
  mac[2] = m2 >> 24;
  mac[3] = m2 >> 16;
  mac[4] = m2 >> 8;
  mac[5] = m2 >> 0;
}

volatile int DCCTimer::minimum_free_memory=__INT_MAX__;

// Return low memory value... 
int DCCTimer::getMinimumFreeMemory() {
  noInterrupts(); // Disable interrupts to get volatile value 
  int retval = freeMemory();
  interrupts();
  return retval;
}

extern "C" char* sbrk(int incr);

int DCCTimer::freeMemory() {
  char top;
  return (int)(&top - reinterpret_cast<char *>(sbrk(0)));
}

#endif