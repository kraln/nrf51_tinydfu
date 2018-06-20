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
#include "app_timer_appsh.h"
#include "app_scheduler.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "ble_hci.h"
#include "ble_radio_notification.h"
#include "pstorage_platform.h"

#define WAIT_TIME 1 /* seconds */

#define APPLICATION_ENTRY 0x00018000 //was: 0x0001B
#define BOOTLOADER_REGION_START 0x0003C000
#define APPLICATION_BUFFER 0x100 /* approx 256 bytes */

uint32_t m_uicr_bootloader_start_address __attribute__((section(".uicrBootStartAddress"))) = BOOTLOADER_REGION_START;
uint8_t application_buffer[APPLICATION_BUFFER]; /* we're just gonna allocate this one up front */

/* Forward Declarations */
bool check_enter_bootloader();
void launch_application();
void sd_init();
void ble_init();
void check_error(uint32_t);
void sd_dispatch(ble_evt_t *);
void sys_evt_dispatch(uint32_t);
void nus_data_handler(ble_nus_t*, uint8_t*, uint16_t);
void on_adv_evt(ble_adv_evt_t);
void _32mhz_clock();

/* Software device stuff */
ble_nus_t  m_nus; /* uart service identifier */
uint16_t   m_conn_handle = BLE_CONN_HANDLE_INVALID; /* connection handle */
ble_uuid_t m_adv_uuids[] = {{BLE_UUID_NUS_SERVICE, BLE_UUID_TYPE_VENDOR_BEGIN}}; /* uuids for uart service */
bool       sd_initialized = false;
volatile bool tx_wait     = false; // send one package and wait until it's recieved
///
/// Entry point
///
int main(void)
{

  /* HW INIT, clocks and so forth */
  _32mhz_clock();
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
  bool should_enter = false;

  // Fast path--check if there is an application. if not, true fast
  uint8_t * ptr = (uint8_t *)APPLICATION_ENTRY;
  if(ptr[3] != 0x20) /* stack pointer */
  {
    return true;
  }

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
  if (sd_initialized) /* should we do this in all cases? */
  {
    /* re-set the vector table base */
    uint32_t err_code = sd_softdevice_vector_table_base_set(APPLICATION_ENTRY);
    check_error(err_code);
  }

  application_entry_t application_entry = *(application_entry_t *)(APPLICATION_ENTRY+4);
  application_entry();
}

/*
 * Memory Layout
 * 0x0003C000 BOOTLOADER_REGION_START
 * 0x00018000 APPLICATION_ENTRY
 * 0x00001000 Softdevice S110 v10
 * 0x00000000 MBR
 *
 * Application size: 0x24000
 */

///
//  Handle Serial RX
///

void serial_rx(uint8_t* data, uint16_t len)
{
  switch (data[0])
  {
    case 'd':
      /* delete page */
      {
        /*
         * Command format: 
         *
         * byte 0: d
         * byte 1: which page to delete
         *
         * Valid pages to delete: 0x00018000 - 0x0003C000
         * Page Number(s) 96 - 239
         */
        if (len != 2)
        {
          {
            const char* test = "! invalid args";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid length */
        }

        if (data[1] < 96 || data[1] >= 240)
        {
          {
            const char* test = "! invalid page";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          } 
          return; /* invalid page */
        }

        uint32_t err = sd_flash_page_erase(data[1]);
        check_error(err);

        application_buffer[60] = 'd';
        application_buffer[61] = data[1];
        application_buffer[62] = 'O';
        application_buffer[63] = 'K';
        ble_nus_string_send(&m_nus, &application_buffer[60], 4);
        check_error(err);

      }
      break;

    case 'w':
      /* write page (assuming erased page) */
      {
        /*
         * Command format: 
         *
         * byte 0: w
         * byte 1: which page to write to
         * byte 2: which portion to write
         * byte 3-18: 16 bytes
         *
         * Valid pages to write: 0x00018000 - 0x0003C000
         * Page Number(s) 96 - 239
         * Portions: 0-63 (16 bytes segments)
         */
        if (len != 19)
        {
          {
            const char* test = "! invalid args";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid length */
        }

        if (data[1] < 96 || data[1] >= 240)
        {
          {
            const char* test = "! invalid page";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid page */
        }

        if (data[2] >= 64)
        {
          {
            const char* test = "! invalid chunk";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid chunk */
        }

        application_buffer[0]  = data[3];
        application_buffer[1]  = data[4];
        application_buffer[2]  = data[5];
        application_buffer[3]  = data[6];
        application_buffer[4]  = data[7];
        application_buffer[5]  = data[8];
        application_buffer[6]  = data[9];
        application_buffer[7]  = data[10];
        application_buffer[8]  = data[11];
        application_buffer[9]  = data[12];
        application_buffer[10] = data[13];
        application_buffer[11] = data[14];
        application_buffer[12] = data[15];
        application_buffer[13] = data[16];
        application_buffer[14] = data[17];
        application_buffer[15] = data[18];

        /* write it out */
        uint32_t err = sd_flash_write((uint32_t *)(data[1] * 1024 + data[2] * 16), (const uint32_t *)&application_buffer[0], 4);
        check_error(err);   

        application_buffer[60] = 'w';
        application_buffer[61] = data[1];
        application_buffer[62] = data[2];
        application_buffer[63] = 'O';
        application_buffer[64] = 'K';
        ble_nus_string_send(&m_nus, &application_buffer[60], 5);
        check_error(err);

      }

      break;
    case 'r': 
      /* read page / chunk */
      {
        /*
         * Command format: 
         *
         * byte 0: d
         * byte 1: which page to read
         * byte 2: which portion to read
         *
         * Valid pages to read: 0x00018000 - 0x0003C000
         * Page Number(s) 96 - 239
         * Portions: 0-63 (16 bytes)
         */
        if (len != 3)
        {
          {
            const char* test = "! invalid args";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid length */
        }

        if (data[1] < 96 || data[1] >= 240)
        {
          {
            const char* test = "! invalid page";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid page */
        }

        if (data[2] >= 128)
        {
          {
            const char* test = "! invalid chunk";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid chunk */
        }

        uint8_t * addr = (uint8_t *) ((data[1]*1024) + (data[2]*16));
        for(uint8_t i = 0; i < 16; i+=4)
        {
          application_buffer[3 + i + 0] = *(addr + i + 0);
          application_buffer[3 + i + 1] = *(addr + i + 1);
          application_buffer[3 + i + 2] = *(addr + i + 2);
          application_buffer[3 + i + 3] = *(addr + i + 3);
        }

        application_buffer[0] = 'r';
        application_buffer[1] = data[1];
        application_buffer[2] = data[2];
       
        uint32_t err = ble_nus_string_send(&m_nus, &application_buffer[0], 16+3);
        check_error(err);
      }

      break;
    case 'i':
      /* get info */
      {
        /*
         * Command format:
         * byte 0: i
         *
         * reads out the DEVICEID and CONFIGID
         */

        if (len != 1)
        {
          {
            const char* test = "! invalid args";
            ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
          }
          return; /* invalid length */
        }

        uint8_t * addr = (uint8_t *) 0x1000005c;
        for(uint8_t i = 0; i < 12; i+=4)
        {
          /* little endians */
          application_buffer[1 + i + 0] = *(addr + i + 3);
          application_buffer[1 + i + 1] = *(addr + i + 2);
          application_buffer[1 + i + 2] = *(addr + i + 1);
          application_buffer[1 + i + 3] = *(addr + i + 0);
        }

        application_buffer[0] = 'i';
       
        uint32_t err = ble_nus_string_send(&m_nus, &application_buffer[0], 12+1);
        check_error(err);
      }
      break;
    case 'e':
      /* echo */
      {
        ble_nus_string_send(&m_nus, data+1, len-1);
      }

      break;
    default:
      /* unknown command */
      {
        const char* test = "! Unknown Cmd";
        ble_nus_string_send(&m_nus, (uint8_t *)test, strlen(test));
      }
  } 
}

void _32mhz_clock()
{
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

  NRF_CLOCK->XTALFREQ = 0xFFFFFF00;
}


///
//
// GROSS NORDIC STUFF HERE
//
///

#define DEVICE_NAME       "KRALN TINYDFU"

#define MIN_CONN_INTERVAL 16   /* units of 1.25ms -> 20ms */
#define MAX_CONN_INTERVAL 60   /* units of 1.25ms -> 75ms */
#define CONN_SUP_TIMEOUT  400  /* units of 10ms -> 4s */
#define APP_ADV_INTERVAL  64   /* units of 0.625ms -> 40ms */
#define APP_ADV_TIMEOUT   180  /* units of 1s -> 3 minutes (max) */

#define FIRST_CONN_PARAMS_UPDATE_DELAY  5000  /* assuming RTC1 prescaler = 0, 5s */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   30000 /* assuming same as above */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3     /* give up after this many attempts */

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

    case BLE_EVT_TX_COMPLETE:
      tx_wait = false;
      break;

    default:
      // No implementation needed.
      break;
  }  

  ble_advertising_on_ble_evt(event);
}


/* current radio state */
volatile bool current_radio_active_state = false;
void ble_on_radio_active_evt(bool radio_active)
{
      current_radio_active_state = radio_active;
}

///
/// Initialize the soft device.
///

void sd_init()
{
  sd_initialized = true;

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
  SOFTDEVICE_HANDLER_APPSH_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_250MS_CALIBRATION, NULL); /* when the second argument is not NULL, it wants a handler? */

  // Enable BLE stack 
  ble_enable_params_t ble_enable_params;
  memset(&ble_enable_params, 0, sizeof(ble_enable_params));
  ble_enable_params.gatts_enable_params.service_changed = 1;
  
  _debug_printf("Enabling bluetooth...");
  err_code = sd_ble_enable(&ble_enable_params);
  check_error(err_code);

  _debug_printf("Setting the ble handler...");
  err_code = softdevice_ble_evt_handler_set(sd_dispatch);
  check_error(err_code);

  _debug_printf("Setting the softdevice handler..");
  err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
  check_error(err_code);

  _debug_printf("Setting up the radio callback");
  err_code = ble_radio_notification_init(NRF_APP_PRIORITY_LOW, NRF_RADIO_NOTIFICATION_DISTANCE_800US, ble_on_radio_active_evt);
  check_error(err_code);

  _debug_printf("Initializing Scheduler...");
  APP_SCHED_INIT(MAX(APP_TIMER_SCHED_EVT_SIZE, 0), 20);

  _debug_printf("Soft device initialized");

}

void sys_evt_dispatch(uint32_t disp)
{
  pstorage_sys_event_handler(disp);
}

void was_error(uint32_t error)
{
  volatile uint32_t e __attribute__((unused)) = error;
}

/* I want to debug on this */
void check_error(uint32_t error)
{
  if (error != NRF_SUCCESS)
  {
    was_error(error);
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

void __attribute__((unused)) app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  /* 
   * id-- identifier
   * pc-- pc at fault
   * info-- more info?
   */
  check_error(id);
}
