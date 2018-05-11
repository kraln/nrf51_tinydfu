#include "hw.h"
#include "debug.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"

/* soft device stuff */

#include "ble.h"
#include "ble_conn_params.h"
#include "ble_advertising.h"
#include "softdevice_handler.h"

#define WAIT_TIME 3 /* seconds */

#define APPLICATION_ENTRY 0x0001B000
#define MAX_APPLICATION_SIZE 0x4D00 /* approx 19k */

uint8_t application_buffer[MAX_APPLICATION_SIZE]; /* we're just gonna allocate this one up front */

/* Forward Declarations */
bool check_enter_bootloader();
void launch_application();
void sd_init();
void check_error(uint32_t);
void sd_dispatch(ble_evt_t*);

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
  sd_init();

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

///
/// Softdevice dispatcher 
///
void sd_dispatch(ble_evt_t* p_ble_evt)
{

  /* first, hand the event to any subsystems that might need it */
  ble_conn_params_on_ble_evt(p_ble_evt);
  ble_advertising_on_ble_evt(p_ble_evt);
  
  /* then, handle anything else here */
  //uint32_t error;

  switch (p_ble_evt->header.evt_id)
  {
    default:
      break;
  }

}


///
/// Initialize the soft device.
///

void sd_init()
{

  /* Define a clock source and hand it to the soft device (in your board) */
  nrf_clock_lf_cfg_t clock_lf_cfg =                    \
  {.source        = NRF_CLOCK_LF_SRC_RC,               \
   .rc_ctiv       = 0,                                 \
   .rc_temp_ctiv  = 0,                                 \
   .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_250_PPM
  }
  ;
  SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

  /* Ask for just one peripheral connection */
  ble_enable_params_t ble_enable_params;
  uint32_t error = softdevice_enable_get_default_config(0,1,&ble_enable_params);
  check_error(error);

  /* Set the MTU size and enable the softdevice */
  error = softdevice_enable(&ble_enable_params);
  check_error(error);

  /* Subscribe to the softdevice newsletter for more events */
  error = softdevice_ble_evt_handler_set(sd_dispatch);
  check_error(error);
}

void check_error(uint32_t error)
{
  if (error != NRF_SUCCESS)
  {
    _debug_printf("Encountered an error (%d), jumping out of bootloader", error);
    launch_application();
  }
}
void app_error_handler_bare(uint32_t error)
{
  check_error(error);
}

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  /* 
   * id-- identifier
   * pc-- pc at fault
   * info-- more info?
   */
  check_error(id);
}
