#include "debug.h"
/* NO-OP */
#define NRF_LOG_ERROR(x,...) do {_debug_printf(x,##__VA_ARGS__);} while(0)
#define NRF_LOG_DEBUG(x,...) do {_debug_printf(x,##__VA_ARGS__);} while(0)
#define NRF_LOG_WARNING(x,...) do {_debug_printf(x,##__VA_ARGS__);} while(0)
