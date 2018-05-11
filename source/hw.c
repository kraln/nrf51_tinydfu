/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, you can obtain one at http://mozilla.org/MPL/2.0/
 *
 */

#include "hw.h"
#include "nrf.h"
#include "nrf51.h"
#include "debug.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_delay.h"

/* Some struct defines so that debuggers know how to read our memory */
volatile NVIC_Type *Interrupts = NVIC;
volatile SCB_Type *SystemControlBlock = SCB;

/* Function to eliminate blocking */
void wait_for_val_ne(volatile uint32_t *value)
{
  uint8_t hold_down = 0xFF;
  while (*value == 0 && hold_down--)
  {
    nrf_delay_us(1);
  }
  if (hold_down)
  {
    return;
  }
  else
  {
    /* Reboot */
    _debug_printf("timed out");
    NVIC_SystemReset();
  }
}

void hw_init()
{
  /* Start the HF clock (XTAL) */
  hw_switch_to_hfclock();

  /* Configure the RAM retention parameters */
  NRF_POWER->RAMON = POWER_RAMON_ONRAM0_RAM0On   << POWER_RAMON_ONRAM0_Pos
                   | POWER_RAMON_ONRAM1_RAM1On  << POWER_RAMON_ONRAM1_Pos
                   | POWER_RAMON_OFFRAM0_RAM0On << POWER_RAMON_OFFRAM0_Pos
                   | POWER_RAMON_OFFRAM1_RAM1On << POWER_RAMON_OFFRAM1_Pos;

  /* Enable Send-Event-on-Send feature (enables WFE's to wake up) */
  SCB->SCR |= SCB_SCR_SEVONPEND_Msk;
}

void hw_clear_port_event()
{
  NRF_GPIOTE->EVENTS_PORT = 0;
}

/* Handle a sense interrupt */
void GPIOTE_IRQHandler(void)
{
  if (gpioFn)
  {
    (*gpioFn)();
  }

  NRF_GPIOTE->EVENTS_PORT = 0;
  NVIC_ClearPendingIRQ(GPIOTE_IRQn);
}

/*
 * Function to switch CPU clock source to internal LFCLK RC oscillator.
 */
void hw_switch_to_lfclock(void)
{
  /* Stop the HF xtal (falls back to the internal RC) */
  NRF_CLOCK->TASKS_HFCLKSTOP = 1;
}

/*
 * Function to start up the HF clk.
 * The CPU switches its clk src automatically .
 */
void hw_switch_to_hfclock(void)
{
  /* Mark the HF clock as not started */
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;

  /* Try to start the HF clock */
  NRF_CLOCK->TASKS_HFCLKSTART = 1;

  /* Start clock */
  wait_for_val_ne(&NRF_CLOCK->EVENTS_HFCLKSTARTED);
}

/*
 * Function for stopping the internal LFCLK RC oscillator.
 */
void hw_stop_LF_clk(void)
{
  /* Stop the LF clock */
  NRF_CLOCK->TASKS_LFCLKSTOP = 1;
}

/*
 * Function for starting the internal LFCLK with RC oscillator as its source.
 */
void hw_start_LF_clk(void)
{
  /* Select the internal 32kHz RC */
  NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos);

  /* Mark the LF Clock as not started */
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;

  /* Try to start the LF clock */
  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  /* Start clock */
  wait_for_val_ne(&NRF_CLOCK->EVENTS_LFCLKSTARTED);
}

void hw_read_reset_reason(uint32_t *resetreas)
{
  /* Read the reason why we reset into state. We like to store it into state so
   * that we can test how our firmware reacts in different simulated reset
   * scenarios. It also makes reading the reason from the debugger easier. */
  *resetreas = NRF_POWER->RESETREAS;

  /* Set the reset reason to 0, because it is a latched register and we have
   * read it and want it to be valid the next time we read it (not contained
   * old latched values from last time we read it). */
  NRF_POWER->RESETREAS = 0;

#if DEBUG_RESET_REASON
  /* Print some messages about why we reset. */

  _debug_printf("I reset because: 0x%08x", *resetreas);

  if (*resetreas & POWER_RESETREAS_DIF_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from entering into debug interface mode");
  }
  if (*resetreas & POWER_RESETREAS_LPCOMP_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from ANADETECT signal from LPCOMP");
  }
  if (*resetreas & POWER_RESETREAS_OFF_Msk)
  {
    _debug_printf("-Reset due to wake-up from OFF mode when wakeup is"
                  " triggered from the DETECT signal from GPIO");
  }
  if (*resetreas & POWER_RESETREAS_LOCKUP_Msk)
  {
    _debug_printf("-Reset from CPU lock-up detected");
  }
  if (*resetreas & POWER_RESETREAS_SREQ_Msk)
  {
    _debug_printf("-Reset from AIRCR.SYSRESETREQ detected");
  }
  if (*resetreas & POWER_RESETREAS_DOG_Msk)
  {
    _debug_printf("-Reset from watchdog detected");
  }
  if (*resetreas & POWER_RESETREAS_RESETPIN_Msk)
  {
    _debug_printf("-Reset from pin detected");
  }
#endif
}

uint32_t hw_ficr_deviceid(size_t index)
{
  return NRF_FICR->DEVICEID[index];
}

void hw_sleep_power_on(void)
{
  /* Set the power mode to power on sleeping, retain some RAM */
  NRF_POWER->RAMON = POWER_RAMON_ONRAM0_Msk | POWER_RAMON_ONRAM1_Msk;

  /* Signal an event and wait for it */
  SEV(); WFE();

  /* Go to sleep, baby */
  WFE();
}

void hw_sleep_power_off(void)
{
  /* Set the power mode to power off sleeping, no RAM retention */
  NRF_POWER->RAMON = POWER_RAMON_OFFRAM0_RAM0Off | POWER_RAMON_OFFRAM1_RAM1Off;

  /* Clear the reset reason, so that it'll be valid when we wake up. */
  NRF_POWER->RESETREAS = 0;

  /* Enter system OFF. After wakeup the chip will be reset */
  NRF_POWER->SYSTEMOFF = 1;
  while(1);
}
