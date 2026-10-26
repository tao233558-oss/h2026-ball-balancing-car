#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#include <stdint.h>

/* Only the OTG FS peripheral driver is used.  The rest of this firmware
 * continues to use ST's Standard Peripheral Library. */
#define HAL_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED

#define USE_HAL_PCD_REGISTER_CALLBACKS 0U

#if !defined(HSE_VALUE)
#define HSE_VALUE 8000000U
#endif

#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif

#if !defined(HSI_VALUE)
#define HSI_VALUE 16000000U
#endif

#if !defined(LSI_VALUE)
#define LSI_VALUE 32000U
#endif

#if !defined(LSE_VALUE)
#define LSE_VALUE 32768U
#endif

#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE 12288000U
#endif

#define VDD_VALUE                 3300U
#define TICK_INT_PRIORITY         0x0FU
#define USE_RTOS                  0U
#define PREFETCH_ENABLE           1U
#define INSTRUCTION_CACHE_ENABLE  1U
#define DATA_CACHE_ENABLE         1U

uint32_t HAL_RCC_GetHCLKFreq(void);

#include "stm32f4xx_hal_pcd.h"

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#ifndef assert_param
#define assert_param(expr) \
    ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#endif
#else
#ifndef assert_param
#define assert_param(expr) ((void)0U)
#endif
#endif

#endif
