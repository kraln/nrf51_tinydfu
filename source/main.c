#include "hw.h"
#include "debug.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

#define WAIT_TIME 3 /* seconds */

#define APPLICATION_ENTRY 0x0001B000
#define MAX_APPLICATION_SIZE 0x4D00 /* approx 19k */

uint8_t application_buffer[MAX_APPLICATION_SIZE]; /* we're just gonna allocate this one up front */

/* Forward Declarations */
bool check_enter_bootloader();
void launch_application();

///
/// Entry point
///
int main(void)
{
  
  /* HW INIT, clocks and so forth */
  hw_init();

  _debug_printf("hardware initialized");
  /* Check to see if we need to enter the bootloader, or just jump to the application */

  if (!check_enter_bootloader())
  {
    launch_application();  
  }

  /* if we got here, we're supposed to do bootloader things */
  
  /* If bootloader: */
  /* init the (yuck) softdevice */
  /* begin advertising */
  
  for(;;);
}

///
/// Determine if we should enter the bootloader or skip directly to the application
/// TODO: customize this logic based on your application
///
bool check_enter_bootloader()
{  
  bool should_enter = false;
  uint32_t accumulated_ms = 0;

  /* Example: set up GPIO directions */
  nrf_gpio_cfg_input(25, NRF_GPIO_PIN_PULLUP);
  nrf_gpio_cfg_input(28, NRF_GPIO_PIN_PULLUP);

  /* Example: wait up to WAIT_TIME seconds for input to happen */
  while (!should_enter && accumulated_ms < WAIT_TIME * 1000UL)
  {    
    /* Example: check if these two pins are low at any time in the first WAIT_TIME seconds */    
    should_enter = (!nrf_gpio_pin_read(28) && !nrf_gpio_pin_read(25));

    nrf_delay_ms(50);
    accumulated_ms += 50;
  }

  return should_enter;
}

///
/// Go to and launch the main application
///

typedef void (*application_entry_t)(void);
void launch_application()
{
  application_entry_t application_entry = *(application_entry_t *)(APPLICATION_ENTRY+4);
  application_entry();
}
