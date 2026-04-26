/*
 * Copyright (c) 2018-2019, CIX Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SKY1_PDC_H
#define SKY1_PDC_H

#define USB_C_SSP_0_HOST_IRQ                            262
#define USB_C_SSP_0_PERIPHERAL_IRQ                      262
#define USB_C_SSP_0_OTG_IRQ                             263

#define USB_C_SSP_1_HOST_IRQ                            268
#define USB_C_SSP_1_PERIPHERAL_IRQ                      268
#define USB_C_SSP_1_OTG_IRQ                             269

#define USB_C_SSP_2_HOST_IRQ                            274
#define USB_C_SSP_2_PERIPHERAL_IRQ                      274
#define USB_C_SSP_2_OTG_IRQ                             275

#define USB_C_SSP_3_HOST_IRQ                            280
#define USB_C_SSP_3_PERIPHERAL_IRQ                      280
#define USB_C_SSP_3_OTG_IRQ                             281

#define USB_SSP_0_HOST_IRQ                              252
#define USB_SSP_0_PERIPHERAL_IRQ                        252
#define USB_SSP_0_OTG_IRQ                               253

#define USB_SSP_1_HOST_IRQ                              257
#define USB_SSP_1_PERIPHERAL_IRQ                        257
#define USB_SSP_1_OTG_IRQ                               258

#define USB2_0_HOST_IRQ                                 240
#define USB2_0_PERIPHERAL_IRQ                           240
#define USB2_0_OTG_IRQ                                  241

#define USB2_1_HOST_IRQ                                 243
#define USB2_1_PERIPHERAL_IRQ                           243
#define USB2_1_OTG_IRQ                                  244

#define USB2_2_HOST_IRQ                                 246
#define USB2_2_PERIPHERAL_IRQ                           246
#define USB2_2_OTG_IRQ                                  247

#define USB2_3_HOST_IRQ                                 249
#define USB2_3_PERIPHERAL_IRQ                           249
#define USB2_3_OTG_IRQ                                  250

#define S5_GPIO_U0                                      372
#define S5_GPIO_U1                                      373
#define S5_GPIO_U2                                      374

#define S5_GPIO_WKUP_EN                                 0x1
#define S5_GPIO_WKUP_DIS                                0x0

#define S5_GPIO_WKUP					BIT(1)
#define USB_C_SSP_3_HOST_WKUP				BIT(3)
#define USB_C_SSP_2_HOST_WKUP				BIT(4)
#define USB_C_SSP_1_HOST_WKUP				BIT(5)
#define USB_C_SSP_0_HOST_WKUP				BIT(6)
#define USB_SSP_1_HOST_WKUP				BIT(7)
#define USB_SSP_0_HOST_WKUP				BIT(8)
#define USB2_3_HOST_WKUP				BIT(9)
#define USB2_2_HOST_WKUP				BIT(10)
#define USB2_1_HOST_WKUP				BIT(11)
#define USB2_0_HOST_WKUP				BIT(12)

void sky1_set_wakeup_enable(unsigned int on, unsigned int wakeup_bit);
int sky1_pdc_handler(u_register_t x1, u_register_t x2, u_register_t x3, u_register_t x4);

#endif /*SKY1_PDC_H */
