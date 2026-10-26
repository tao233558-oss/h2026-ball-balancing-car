/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)


#define GPIO_HFXT_PORT                                                     GPIOA
#define GPIO_HFXIN_PIN                                             DL_GPIO_PIN_5
#define GPIO_HFXIN_IOMUX                                         (IOMUX_PINCM10)
#define GPIO_HFXOUT_PIN                                            DL_GPIO_PIN_6
#define GPIO_HFXOUT_IOMUX                                        (IOMUX_PINCM11)
#define CPUCLK_FREQ                                                     80000000
/* Defines for SYSPLL_ERR_01 Workaround */
/* Represent 1.000 as 1000 */
#define FLOAT_TO_INT_SCALE                                               (1000U)
#define FCC_EXPECTED_RATIO                                                  2500
#define FCC_UPPER_BOUND                       (FCC_EXPECTED_RATIO * (1 + 0.003))
#define FCC_LOWER_BOUND                       (FCC_EXPECTED_RATIO * (1 - 0.003))

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);


/* Defines for DCC_100_PWM2 */
#define DCC_100_PWM2_INST                                                  TIMG0
#define DCC_100_PWM2_INST_IRQHandler                            TIMG0_IRQHandler
#define DCC_100_PWM2_INST_INT_IRQN                              (TIMG0_INT_IRQn)
#define DCC_100_PWM2_INST_CLK_FREQ                                       5000000
/* GPIO defines for channel 0 */
#define GPIO_DCC_100_PWM2_C0_PORT                                          GPIOA
#define GPIO_DCC_100_PWM2_C0_PIN                                  DL_GPIO_PIN_12
#define GPIO_DCC_100_PWM2_C0_IOMUX                               (IOMUX_PINCM34)
#define GPIO_DCC_100_PWM2_C0_IOMUX_FUNC              IOMUX_PINCM34_PF_TIMG0_CCP0
#define GPIO_DCC_100_PWM2_C0_IDX                             DL_TIMER_CC_0_INDEX

/* Defines for DCC_100_PWM1 */
#define DCC_100_PWM1_INST                                                  TIMA1
#define DCC_100_PWM1_INST_IRQHandler                            TIMA1_IRQHandler
#define DCC_100_PWM1_INST_INT_IRQN                              (TIMA1_INT_IRQn)
#define DCC_100_PWM1_INST_CLK_FREQ                                       5000000
/* GPIO defines for channel 0 */
#define GPIO_DCC_100_PWM1_C0_PORT                                          GPIOA
#define GPIO_DCC_100_PWM1_C0_PIN                                  DL_GPIO_PIN_28
#define GPIO_DCC_100_PWM1_C0_IOMUX                                (IOMUX_PINCM3)
#define GPIO_DCC_100_PWM1_C0_IOMUX_FUNC               IOMUX_PINCM3_PF_TIMA1_CCP0
#define GPIO_DCC_100_PWM1_C0_IDX                             DL_TIMER_CC_0_INDEX



/* Defines for SPI_0 */
#define SPI_0_INST                                                         SPI0
#define SPI_0_INST_IRQHandler                                   SPI0_IRQHandler
#define SPI_0_INST_INT_IRQN                                       SPI0_INT_IRQn
#define GPIO_SPI_0_PICO_PORT                                              GPIOA
#define GPIO_SPI_0_PICO_PIN                                       DL_GPIO_PIN_9
#define GPIO_SPI_0_IOMUX_PICO                                   (IOMUX_PINCM20)
#define GPIO_SPI_0_IOMUX_PICO_FUNC                   IOMUX_PINCM20_PF_SPI0_PICO
#define GPIO_SPI_0_POCI_PORT                                              GPIOB
#define GPIO_SPI_0_POCI_PIN                                      DL_GPIO_PIN_19
#define GPIO_SPI_0_IOMUX_POCI                                   (IOMUX_PINCM45)
#define GPIO_SPI_0_IOMUX_POCI_FUNC                   IOMUX_PINCM45_PF_SPI0_POCI
/* GPIO configuration for SPI_0 */
#define GPIO_SPI_0_SCLK_PORT                                              GPIOB
#define GPIO_SPI_0_SCLK_PIN                                      DL_GPIO_PIN_18
#define GPIO_SPI_0_IOMUX_SCLK                                   (IOMUX_PINCM44)
#define GPIO_SPI_0_IOMUX_SCLK_FUNC                   IOMUX_PINCM44_PF_SPI0_SCLK



/* Port definition for Pin Group ji1guang1 */
#define ji1guang1_PORT                                                   (GPIOA)

/* Defines for ji1guang1_PIN: GPIOA.17 with pinCMx 39 on package pin 10 */
#define ji1guang1_ji1guang1_PIN_PIN                             (DL_GPIO_PIN_17)
#define ji1guang1_ji1guang1_PIN_IOMUX                            (IOMUX_PINCM39)
/* Defines for OLED_SPI_RES: GPIOA.8 with pinCMx 19 on package pin 54 */
#define OLED_SPI_OLED_SPI_RES_PORT                                       (GPIOA)
#define OLED_SPI_OLED_SPI_RES_PIN                                (DL_GPIO_PIN_8)
#define OLED_SPI_OLED_SPI_RES_IOMUX                              (IOMUX_PINCM19)
/* Defines for OLED_SPI_DC: GPIOB.2 with pinCMx 15 on package pin 50 */
#define OLED_SPI_OLED_SPI_DC_PORT                                        (GPIOB)
#define OLED_SPI_OLED_SPI_DC_PIN                                 (DL_GPIO_PIN_2)
#define OLED_SPI_OLED_SPI_DC_IOMUX                               (IOMUX_PINCM15)
/* Defines for OLED_SPI_CS: GPIOA.2 with pinCMx 7 on package pin 42 */
#define OLED_SPI_OLED_SPI_CS_PORT                                        (GPIOA)
#define OLED_SPI_OLED_SPI_CS_PIN                                 (DL_GPIO_PIN_2)
#define OLED_SPI_OLED_SPI_CS_IOMUX                                (IOMUX_PINCM7)
/* Defines for KEY_10: GPIOB.3 with pinCMx 16 on package pin 51 */
#define KEY_KEY_10_PORT                                                  (GPIOB)
#define KEY_KEY_10_PIN                                           (DL_GPIO_PIN_3)
#define KEY_KEY_10_IOMUX                                         (IOMUX_PINCM16)
/* Defines for KEY_1: GPIOB.6 with pinCMx 23 on package pin 58 */
#define KEY_KEY_1_PORT                                                   (GPIOB)
#define KEY_KEY_1_PIN                                            (DL_GPIO_PIN_6)
#define KEY_KEY_1_IOMUX                                          (IOMUX_PINCM23)
/* Defines for KEY_2: GPIOB.7 with pinCMx 24 on package pin 59 */
#define KEY_KEY_2_PORT                                                   (GPIOB)
#define KEY_KEY_2_PIN                                            (DL_GPIO_PIN_7)
#define KEY_KEY_2_IOMUX                                          (IOMUX_PINCM24)
/* Defines for KEY_3: GPIOA.26 with pinCMx 59 on package pin 30 */
#define KEY_KEY_3_PORT                                                   (GPIOA)
#define KEY_KEY_3_PIN                                           (DL_GPIO_PIN_26)
#define KEY_KEY_3_IOMUX                                          (IOMUX_PINCM59)
/* Defines for KEY_4: GPIOA.25 with pinCMx 55 on package pin 26 */
#define KEY_KEY_4_PORT                                                   (GPIOA)
#define KEY_KEY_4_PIN                                           (DL_GPIO_PIN_25)
#define KEY_KEY_4_IOMUX                                          (IOMUX_PINCM55)
/* Defines for DIR2: GPIOA.13 with pinCMx 35 on package pin 6 */
#define STEP_MOTOR_DIR2_PORT                                             (GPIOA)
#define STEP_MOTOR_DIR2_PIN                                     (DL_GPIO_PIN_13)
#define STEP_MOTOR_DIR2_IOMUX                                    (IOMUX_PINCM35)
/* Defines for DCY2: GPIOA.14 with pinCMx 36 on package pin 7 */
#define STEP_MOTOR_DCY2_PORT                                             (GPIOA)
#define STEP_MOTOR_DCY2_PIN                                     (DL_GPIO_PIN_14)
#define STEP_MOTOR_DCY2_IOMUX                                    (IOMUX_PINCM36)
/* Defines for SLP2: GPIOA.15 with pinCMx 37 on package pin 8 */
#define STEP_MOTOR_SLP2_PORT                                             (GPIOA)
#define STEP_MOTOR_SLP2_PIN                                     (DL_GPIO_PIN_15)
#define STEP_MOTOR_SLP2_IOMUX                                    (IOMUX_PINCM37)
/* Defines for RST2: GPIOA.16 with pinCMx 38 on package pin 9 */
#define STEP_MOTOR_RST2_PORT                                             (GPIOA)
#define STEP_MOTOR_RST2_PIN                                     (DL_GPIO_PIN_16)
#define STEP_MOTOR_RST2_IOMUX                                    (IOMUX_PINCM38)
/* Defines for DIR1: GPIOA.31 with pinCMx 6 on package pin 39 */
#define STEP_MOTOR_DIR1_PORT                                             (GPIOA)
#define STEP_MOTOR_DIR1_PIN                                     (DL_GPIO_PIN_31)
#define STEP_MOTOR_DIR1_IOMUX                                     (IOMUX_PINCM6)
/* Defines for DCY1: GPIOB.24 with pinCMx 52 on package pin 23 */
#define STEP_MOTOR_DCY1_PORT                                             (GPIOB)
#define STEP_MOTOR_DCY1_PIN                                     (DL_GPIO_PIN_24)
#define STEP_MOTOR_DCY1_IOMUX                                    (IOMUX_PINCM52)
/* Defines for SLP1: GPIOB.20 with pinCMx 48 on package pin 19 */
#define STEP_MOTOR_SLP1_PORT                                             (GPIOB)
#define STEP_MOTOR_SLP1_PIN                                     (DL_GPIO_PIN_20)
#define STEP_MOTOR_SLP1_IOMUX                                    (IOMUX_PINCM48)
/* Defines for RST1: GPIOA.7 with pinCMx 14 on package pin 49 */
#define STEP_MOTOR_RST1_PORT                                             (GPIOA)
#define STEP_MOTOR_RST1_PIN                                      (DL_GPIO_PIN_7)
#define STEP_MOTOR_RST1_IOMUX                                    (IOMUX_PINCM14)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);

bool SYSCFG_DL_SYSCTL_SYSPLL_init(void);
void SYSCFG_DL_DCC_100_PWM2_init(void);
void SYSCFG_DL_DCC_100_PWM1_init(void);
void SYSCFG_DL_SPI_0_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
