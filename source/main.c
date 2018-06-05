#include "hw.h"
#include "debug.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_mbr.h"

/* soft device stuff */

#include "ble.h"
#include "ble_conn_params.h"
#include "ble_advertising.h"
#include "softdevice_handler.h"
#include "softdevice_handler_appsh.h"
#include "app_scheduler.h"

#define WAIT_TIME 3 /* seconds */

#define APPLICATION_ENTRY 0x0001B000
#define BOOTLOADER_REGION_START 0x0003C000
#define MAX_APPLICATION_SIZE 0x4D00 /* approx 19k */

uint32_t m_uicr_bootloader_start_address __attribute__((section(".uicrBootStartAddress"))) = BOOTLOADER_REGION_START;
uint8_t application_buffer[MAX_APPLICATION_SIZE]; /* we're just gonna allocate this one up front */

/* Forward Declarations */
bool check_enter_bootloader();
void launch_application();
void sd_init();
void check_error(uint32_t);
void sd_dispatch(uint32_t);

///
/// Entry point
///
int main(void)
{

#define _32MHZ_CLOCK 1
#if _32MHZ_CLOCK
  /* Configure for the 32MHz Clock, as per Taiyo-Yuden Datasheet */

  /* First, check if it's not already set */
  if (*(uint32_t *)0x10001008 == 0xFFFFFFFF) 
  { 
    _debug_printf("setting clock to 32mhz");

    /* wait for it to not be busy */
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos; 
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy){} 
    
    /* Configure for the proper 32mhz clock */
    *(uint32_t *)0x10001008 = 0xFFFFFF00; 
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos; 
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy){} 

    /* Reset */
    NVIC_SystemReset(); 
    while (true){} 
  } 
 
#endif

  /* HW INIT, clocks and so forth */
  hw_init();

  _debug_printf("hardware initialized");
  /* Check to see if we need to enter the bootloader, or just jump to the application */

  if (!check_enter_bootloader())
  {
    _debug_printf("launching application");
    launch_application();  
  }

  /* if we got here, we're supposed to do bootloader things */
  _debug_printf("entering bootloader");
  /* If bootloader: */
  /* init the (yuck) softdevice */
  sd_init();

  /* begin advertising */
  _debug_printf("entering powersave loop");
  for(;;);
}

///
/// Determine if we should enter the bootloader or skip directly to the application
/// TODO: customize this logic based on your application
///
bool check_enter_bootloader()
{  
  return true; // XXX editing debugger for now

  bool should_enter = false;


  // TODO: Fast path--check if there is an application. if not, true fast


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
void sd_dispatch(uint32_t event)
{
  // do stuff
}


///
/// Initialize the soft device.
///

void sd_init()
{

  uint32_t         err_code;
  sd_mbr_command_t com = {SD_MBR_COMMAND_INIT_SD, };

  _debug_printf("Initializing the softdevice handlers...");
 
  /* Initialize Softdevice */ 
  err_code = sd_mbr_command(&com);
  check_error(err_code); 

  _debug_printf("Setting vector table base...");
  /* Set vector table base */
  err_code = sd_softdevice_vector_table_base_set(BOOTLOADER_REGION_START);
  check_error(err_code);

  _debug_printf("Setting clock source...");
  /* Give it the clock config */
  SOFTDEVICE_HANDLER_APPSH_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION, true);

  // Enable BLE stack 
  ble_enable_params_t ble_enable_params;
  memset(&ble_enable_params, 0, sizeof(ble_enable_params));
  ble_enable_params.gatts_enable_params.service_changed = 1;
  
  _debug_printf("Enabling bluetooth...");
  err_code = sd_ble_enable(&ble_enable_params);
  check_error(err_code);

  _debug_printf("Setting the handler...");
  err_code = softdevice_sys_evt_handler_set(sd_dispatch);
  check_error(err_code);

  _debug_printf("Initializing Scheduler...");
  APP_SCHED_INIT(0, 20);

  _debug_printf("Soft device initialized");

}

void check_error(uint32_t error)
{
  if (error != NRF_SUCCESS)
  {
    _debug_printf("Return code (%d) not success (%d)", error, NRF_SUCCESS);
  } else {
    _debug_printf("Check: No error (%d)", error);
  }
  
}

void app_error_handler(uint32_t error, uint32_t x, const uint8_t* y)
{
  check_error(error);
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
  _debug_printf("app error fault handler");
  check_error(id);
}
