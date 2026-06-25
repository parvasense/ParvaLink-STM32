#ifndef DEBUG_H_
#define DEBUG_H_

#include "Uart.h"

// Comment this out for a lean/production build
//#define DEBUG_VERBOSE

#ifdef DEBUG_VERBOSE
    #define DBG(str) Uart2_Debug(str)
#else
    #define DBG(str)
#endif

#endif /* DEBUG_H_ */
