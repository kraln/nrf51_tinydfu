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
#include "app_timer.h"
#include "ble_nus.h"
#include "ble_hci.h"

#define WAIT_TIME 3 /* seconds */

#define APPLICATION_ENTRY 0x0001B000
#define BOOTLOADER_REGION_START 0x0003C000
#define APPLICATION_BUFFER 0x1000 /* approx 4k */

uint32_t m_uicr_bootloader_start_address __attribute__((section(".uicrBootStartAddress"))) = BOOTLOADER_REGION_START;
uint8_t application_buffer[APPLICATION_BUFFER]; /* we're just gonna allocate this one up front */

/* Forward Declarations */
bool check_enter_bootloader();
void launch_application();
void sd_init();
void ble_init();
void check_error(uint32_t);
void sd_dispatch(ble_evt_t *);
void nus_data_handler(ble_nus_t*, uint8_t*, uint16_t);
void on_adv_evt(ble_adv_evt_t);

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
  ble_init();

  /* begin advertising */
  _debug_printf("beginning advertising");
  uint32_t err = ble_advertising_start(BLE_ADV_MODE_FAST);
  check_error(err);

  _debug_printf("entering powersave loop");
  for(;;)
  {
    err = sd_app_evt_wait();
    check_error(err);
  }
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
//  Handle Serial RX
///

void serial_rx(uint8_t* data, uint16_t len)
{
 _debug_printf("Got data len %d bytes", len);

 // app_uart_put -> reply
}


///
//
// GROSS NORDIC STUFF HERE
//
///

#define DEVICE_NAME       "KRALN TINYDFU"

#define MIN_CONN_INTERVAL 16   /* units of 1.25ms -> 20ms */
#define MAX_CONN_INTERVAL 80   /* units of 1.25ms -> 100ms */
#define CONN_SUP_TIMEOUT  400  /* units of 10ms -> 4s */
#define APP_ADV_INTERVAL  64   /* units of 0.625ms -> 40ms */
#define APP_ADV_TIMEOUT   180  /* units of 1s -> 3 minutes (max) */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  5000  /* assuming RTC1 prescaler = 0, 5s */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   30000 /* assuming same as above */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3     /* give up after this many attempts */

ble_nus_t  m_nus; /* uart service identifier */
uint16_t   m_conn_handle = BLE_CONN_HANDLE_INVALID; /* connection handle */
ble_uuid_t m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}}; /* uuids for uart service */

void sd_conn_params(ble_conn_params_evt_t* p_evt)
{
  if(p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
  {
    _debug_printf("! BLE connection parameters failed, disconnecting");
    uint32_t error = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    check_error(error);
  }
}

void sd_conn_error(uint32_t e)
{
  _debug_printf("Connection error!");
  check_error(e);
}

void ble_init()
{
  
  uint32_t                err_code;
  ble_gap_conn_params_t   gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  _debug_printf("Initializing GAP parameters...");

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode,
      (const uint8_t *) DEVICE_NAME,
      strlen(DEVICE_NAME));
  check_error(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));

  gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
  gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
  gap_conn_params.slave_latency     = 0;
  gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  check_error(err_code);

  _debug_printf("Initializing Services...");
  
  /* We're just using the serial service here */
  ble_nus_init_t nus_init;
  memset(&nus_init, 0, sizeof(nus_init));
  nus_init.data_handler = nus_data_handler;
  err_code = ble_nus_init(&m_nus, &nus_init);
  check_error(err_code);

  _debug_printf("Initializing Advertising...");
  ble_advdata_t advdata;
  ble_advdata_t scanrsp;

  /* Set up the advertising struct */
  memset(&advdata, 0, sizeof(advdata));
  advdata.name_type          = BLE_ADVDATA_FULL_NAME;
  advdata.include_appearance = false;
  advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;

  memset(&scanrsp, 0, sizeof(scanrsp));
  scanrsp.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
  scanrsp.uuids_complete.p_uuids  = m_adv_uuids;

  ble_adv_modes_config_t options = {0};
  options.ble_adv_fast_enabled  = BLE_ADV_FAST_ENABLED;
  options.ble_adv_fast_interval = APP_ADV_INTERVAL;
  options.ble_adv_fast_timeout  = APP_ADV_TIMEOUT;

  err_code = ble_advertising_init(&advdata, &scanrsp, &options, on_adv_evt, NULL);
  check_error(err_code);

  _debug_printf("Initializing connection parameters...");
  ble_conn_params_init_t cp_init;

  memset(&cp_init, 0, sizeof(cp_init));

  cp_init.p_conn_params                  = NULL;
  cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
  cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
  cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
  cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
  cp_init.disconnect_on_fail             = false;
  cp_init.evt_handler                    = sd_conn_params;
  cp_init.error_handler                  = sd_conn_error;

  err_code = ble_conn_params_init(&cp_init);
  check_error(err_code);
}

///
/// Callback from uart service
///
void nus_data_handler(ble_nus_t * p_nus, uint8_t * p_data, uint16_t length)
{
  /* pass through to application logic */
  serial_rx(p_data, length);
}

///
/// Callback from advertising event
///
void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
  switch (ble_adv_evt)
  {
    case BLE_ADV_EVT_FAST:
      _debug_printf("Advertising begun");
      break;
    case BLE_ADV_EVT_IDLE:
      _debug_printf("Advertising timeout");
      break;
    default:
      break;
  }
}

///
/// Softdevice dispatcher 
///
void sd_dispatch(ble_evt_t * event)
{
  uint32_t err_code;

  ble_conn_params_on_ble_evt(event);
  ble_nus_on_ble_evt(&m_nus, event);

  switch (event->header.evt_id)
  {
    case BLE_GAP_EVT_CONNECTED:
      _debug_printf("Connected");
      m_conn_handle = event->evt.gap_evt.conn_handle;
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      _debug_printf("Disconnected");
      m_conn_handle = BLE_CONN_HANDLE_INVALID;
      break;

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      // Pairing not supported
      err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
      check_error(err_code);
      break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      // No system attributes have been stored.
      err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
      check_error(err_code);
      break;

    default:
      // No implementation needed.
      break;
  }  

  ble_advertising_on_ble_evt(event);
}


///
/// Initialize the soft device.
///

void sd_init()
{

  uint32_t         err_code;
  sd_mbr_command_t com = {SD_MBR_COMMAND_INIT_SD, };

  /* starting the timer, needed for advertising */
  APP_TIMER_INIT(0, 4, false); 

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
  err_code = softdevice_ble_evt_handler_set(sd_dispatch);
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
  check_error(id);
}
