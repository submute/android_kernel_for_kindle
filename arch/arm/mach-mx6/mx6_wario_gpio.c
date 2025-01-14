/*
 * Copyright 2012 Amazon.com, Inc. All rights reserved.
 */
/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

 /*
  * Wario GPIO file for dynamic gpio configuration
  */

#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/fsl_devices.h>

#include <mach/boardid.h>
#include "board-mx6sl_wario.h"
#include "wario_iomux/include/iomux_define.h"
#include "wario_iomux/include/iomux_register.h"

static int gpio_wan_ldo_en = 0;

int gpio_max44009_als_int(void)
{
	return gpio_to_irq(MX6_WARIO_ALS_INT);
}
EXPORT_SYMBOL(gpio_max44009_als_int);

int wario_als_gpio_init(void)
{
	int ret = 0;
	ret = gpio_request(MX6_WARIO_ALS_INT, "als_max44009");
	if(unlikely(ret)) return ret;

	ret = gpio_direction_input(MX6_WARIO_ALS_INT);
	if(unlikely(ret)) goto free_als_gpio;

	return ret;
free_als_gpio:
	gpio_free(MX6_WARIO_ALS_INT);
	return ret;
}
EXPORT_SYMBOL(wario_als_gpio_init);

int gpio_accelerometer_int1(void)
{
	return gpio_to_irq(MX6_WARIO_ACCINT1);
}
EXPORT_SYMBOL(gpio_accelerometer_int1);

int gpio_accelerometer_int2(void)
{
	return gpio_to_irq(MX6_WARIO_ACCINT2);
}
EXPORT_SYMBOL(gpio_accelerometer_int2);

int gpio_hallsensor_irq(void)
{
	return gpio_to_irq(MX6_WARIO_HALL_SNS);
}
EXPORT_SYMBOL(gpio_hallsensor_irq);

/* proximity sensor related */

int wario_prox_gpio_init(void)
{
	int ret = 0;

	ret = gpio_request(MX6_WARIO_PROX_INT, "prox_int");
	if (ret) return ret;
	ret = gpio_request(MX6_WARIO_PROX_RST, "prox_rst");
	if (ret) {
		gpio_free(MX6_WARIO_PROX_INT);
		return ret;
	}

	ret = gpio_direction_input(MX6_WARIO_PROX_INT);
	if (ret) goto err_exit;
	ret = gpio_direction_output(MX6_WARIO_PROX_RST, 0);
	if (ret) goto err_exit;

	return ret;

err_exit:
	gpio_free(MX6_WARIO_PROX_INT);
	gpio_free(MX6_WARIO_PROX_RST);

	return ret;
}
EXPORT_SYMBOL(wario_prox_gpio_init);

int gpio_proximity_int(void)
{
	return gpio_to_irq(MX6_WARIO_PROX_INT);
}
EXPORT_SYMBOL(gpio_proximity_int);

int gpio_proximity_detected(void)
{
	return gpio_get_value(MX6_WARIO_PROX_INT);
}
EXPORT_SYMBOL(gpio_proximity_detected);

void gpio_proximity_reset(void)
{
	gpio_set_value(MX6_WARIO_PROX_RST, 0);
	msleep(10);
	gpio_set_value(MX6_WARIO_PROX_RST, 1);
	msleep(10);
}
EXPORT_SYMBOL(gpio_proximity_reset);

void wario_fsr_init_pins(void)
{
	gpio_request(MX6SL_ARM2_EPDC_SDDO_8, "fsr_wake_int");
	gpio_request(MX6SL_ARM2_EPDC_SDDO_9, "fsr_reset");
	gpio_request(MX6SL_ARM2_EPDC_SDDO_10, "fsr_sqze_int");
	gpio_request(MX6SL_ARM2_EPDC_SDDO_11, "fsr_swd_clk");
	gpio_request(MX6SL_ARM2_EPDC_SDDO_12, "fsr_swdio");

	if(lab126_board_rev_greater_eq(BOARD_ID_WARIO_2)) //fsr spare is different on wario
	{
		gpio_request(MX6SL_PIN_KEY_COL1, "fsr_spare");
		gpio_direction_output(MX6SL_PIN_KEY_COL1, 1);
	}
	else if (lab126_board_is(BOARD_ID_ICEWINE_WARIO) ||
		lab126_board_is(BOARD_ID_ICEWINE_WFO_WARIO) ||
		lab126_board_is(BOARD_ID_ICEWINE_WARIO_512) ||
		lab126_board_is(BOARD_ID_ICEWINE_WFO_WARIO_512))

	{
		gpio_request(MX6SL_ARM2_EPDC_SDDO_13, "fsr_spare");
		gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_13, 1);
	}
	gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_8);
	gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_9, 1);
	gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_10);
}
EXPORT_SYMBOL(wario_fsr_init_pins);

void fsr_set_pin(int which, int enable)
{
	switch(which)
	{
		case 8:
			gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_8, enable);
			break;
		case 9:
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_9, enable);
			break;
		case 10:
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_10, enable);
			break;
		case 11:
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_11, enable);
			break;
		case 12:
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_12, enable);
			break;
		case 13:
			gpio_set_value(MX6SL_PIN_KEY_COL1, enable);
			break;
		default:
		break;
	}
}
EXPORT_SYMBOL(fsr_set_pin);

void fsr_wake_pin_pta5(int enable)
{
	if(lab126_board_rev_greater_eq(BOARD_ID_WARIO_2)) //fsr spare is different on wario
	{
		if(enable) {
			gpio_set_value(MX6SL_PIN_KEY_COL1, 0);
		}else{
			gpio_set_value(MX6SL_PIN_KEY_COL1, 1);
		}
	}else
	{
		if(enable) {
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_13, 0);
		}else{
			gpio_set_value(MX6SL_ARM2_EPDC_SDDO_13, 1);
		}
	}
	udelay(100);
}
EXPORT_SYMBOL(fsr_wake_pin_pta5);

void fsr_pm_pins(int suspend_resume)
{
	if(lab126_board_rev_greater_eq(BOARD_ID_WARIO_2)) //fsr spare is different on wario
	{
		if(suspend_resume == 0) {
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_8);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_9);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_10);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_11);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_12);
			gpio_direction_input(MX6SL_PIN_KEY_COL1);
		}else {
			gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_9, 1);
			gpio_direction_output(MX6SL_PIN_KEY_COL1, 1);
		}
	}else
	{
		if(suspend_resume == 0) {
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_8);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_9);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_10);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_11);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_12);
			gpio_direction_input(MX6SL_ARM2_EPDC_SDDO_13);
		}else {
			gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_9, 1);
			gpio_direction_output(MX6SL_ARM2_EPDC_SDDO_13, 1);
		}
	}
	udelay(100);
}
EXPORT_SYMBOL(fsr_pm_pins);




int fsr_get_irq_state(void)
{
	return gpio_get_value(MX6SL_ARM2_EPDC_SDDO_10);
}
EXPORT_SYMBOL(fsr_get_irq_state);

void gpio_fsr_reset(void)
{
	gpio_set_value(MX6SL_ARM2_EPDC_SDDO_9, 0);
	msleep(1);
	gpio_set_value(MX6SL_ARM2_EPDC_SDDO_9, 1);
	msleep(1);
}
EXPORT_SYMBOL(gpio_fsr_reset);

int gpio_fsr_button_irq(void)
{
	return gpio_to_irq(MX6SL_ARM2_EPDC_SDDO_10);
}
EXPORT_SYMBOL(gpio_fsr_button_irq);

int gpio_fsr_bootloader_irq(void)
{
	return gpio_to_irq(MX6SL_ARM2_EPDC_SDDO_8);
}
EXPORT_SYMBOL(gpio_fsr_bootloader_irq);

int gpio_fsr_logging_irq(void)
{
        if(lab126_board_rev_greater_eq(BOARD_ID_WARIO_2)) //fsr spare is different on wario
                return gpio_to_irq(MX6SL_PIN_KEY_COL1);
        else
                return gpio_to_irq(MX6SL_ARM2_EPDC_SDDO_13);
}
EXPORT_SYMBOL(gpio_fsr_logging_irq);

/*
 * hall sensor gpio value hi is not detected, lo is detected
 */
int gpio_hallsensor_detect(void)
{
	return !gpio_get_value(MX6_WARIO_HALL_SNS);
}
EXPORT_SYMBOL(gpio_hallsensor_detect);

void gpio_hallsensor_pullup(int enable)
{
	if (enable > 0) {
		mxc_iomux_v3_setup_pad(MX6SL_HALL_SNS(PU));
	} else {
		mxc_iomux_v3_setup_pad(MX6SL_HALL_SNS(PD));
	}
}
EXPORT_SYMBOL(gpio_hallsensor_pullup);

/* WiFi Power Enable/Disable */
void gpio_wifi_power_enable(int enable)
{
	gpio_direction_output(MX6_WARIO_WIFI_PWD, 0);
        gpio_set_value(MX6_WARIO_WIFI_PWD, enable);
}
EXPORT_SYMBOL(gpio_wifi_power_enable);

/**************touch**************************/
int gpio_cyttsp_init_pins(void)
{
	int ret = 0;
	//touch pins
	ret = gpio_request(MX6SL_PIN_TOUCH_INTB, "touch_intb");
	if(unlikely(ret)) return ret;

	ret = gpio_request(MX6SL_PIN_TOUCH_RST, "touch_rst");
	if(unlikely(ret)) goto free_intb;

	gpio_direction_input(MX6SL_PIN_TOUCH_INTB);
	gpio_direction_output(MX6SL_PIN_TOUCH_RST, 1);

	return ret;
free_intb:
	gpio_free(MX6SL_PIN_TOUCH_INTB);
	return ret;

}
EXPORT_SYMBOL(gpio_cyttsp_init_pins);

/* zforce2 GPIOs setup */
int gpio_zforce_init_pins(void)
{
	int ret = 0;
	
	ret = gpio_request(MX6SL_PIN_TOUCH_INTB, "touch_intb");
	if(unlikely(ret)) return ret;

	ret = gpio_request(MX6SL_PIN_TOUCH_RST, "touch_rst");
	if(unlikely(ret)) goto free_intb;

	/* touch BSL programming pins */
	ret = gpio_request(MX6SL_PIN_TOUCH_SWDL, "touch_swdl");
	if(unlikely(ret)) goto free_rst;
	
	ret = gpio_request(MX6SL_PIN_TOUCH_UART_TX, "touch_uarttx");
	if(unlikely(ret)) goto free_swdl;
	
	ret = gpio_request(MX6SL_PIN_TOUCH_UART_RX, "touch_uartrx");
	if(unlikely(ret)) goto free_uarttx;

	/* GPIO Interrupt - is set as input once and for all */
	gpio_direction_input(MX6SL_PIN_TOUCH_INTB);
	
	/* trigger reset - active low */
	gpio_direction_output(MX6SL_PIN_TOUCH_RST, 0);

	return ret;

free_uarttx:
	gpio_free(MX6SL_PIN_TOUCH_UART_TX);
free_swdl:
	gpio_free(MX6SL_PIN_TOUCH_SWDL);
free_rst:
	gpio_free(MX6SL_PIN_TOUCH_RST);
free_intb:
	gpio_free(MX6SL_PIN_TOUCH_INTB);
	return ret;
}
EXPORT_SYMBOL(gpio_zforce_init_pins);

int gpio_zforce_reset_ena(int enable)
{
	if(enable)
		gpio_direction_output(MX6SL_PIN_TOUCH_RST, 0);
	else
		gpio_direction_input(MX6SL_PIN_TOUCH_RST);
	return 0;
}
EXPORT_SYMBOL(gpio_zforce_reset_ena);

void gpio_zforce_set_reset(int val)
{
	gpio_set_value(MX6SL_PIN_TOUCH_RST, val);
}
EXPORT_SYMBOL(gpio_zforce_set_reset);

void gpio_zforce_set_bsl_test(int val)
{
        gpio_set_value(MX6SL_PIN_TOUCH_SWDL, val);
}
EXPORT_SYMBOL(gpio_zforce_set_bsl_test);

void gpio_zforce_bslpins_ena(int enable)
{
	if(enable) {
		gpio_direction_output(MX6SL_PIN_TOUCH_SWDL, 0);
		gpio_direction_input(MX6SL_PIN_TOUCH_UART_RX);
		gpio_direction_output(MX6SL_PIN_TOUCH_UART_TX, 0);
	} else {
		gpio_direction_input(MX6SL_PIN_TOUCH_SWDL);
		gpio_direction_input(MX6SL_PIN_TOUCH_UART_TX);
		gpio_direction_input(MX6SL_PIN_TOUCH_UART_RX);
	}
}
EXPORT_SYMBOL(gpio_zforce_bslpins_ena);

void gpio_zforce_free_pins(void)
{
	gpio_free(MX6SL_PIN_TOUCH_INTB);
	gpio_free(MX6SL_PIN_TOUCH_RST);
	gpio_free(MX6SL_PIN_TOUCH_SWDL);
	gpio_free(MX6SL_PIN_TOUCH_UART_TX);
	gpio_free(MX6SL_PIN_TOUCH_UART_RX);
}
EXPORT_SYMBOL(gpio_zforce_free_pins);

/** Touch Controller IRQ setup for cyttsp and zforce2 **/
void gpio_touchcntrl_request_irq(int enable)
{
	if (enable)
		gpio_direction_input(MX6SL_PIN_TOUCH_INTB);
}
EXPORT_SYMBOL(gpio_touchcntrl_request_irq);

int gpio_touchcntrl_irq(void)
{
	return gpio_to_irq(MX6SL_PIN_TOUCH_INTB);
}
EXPORT_SYMBOL(gpio_touchcntrl_irq);

int gpio_touchcntrl_irq_get_value(void)
{
	return gpio_get_value( MX6SL_PIN_TOUCH_INTB);
}
EXPORT_SYMBOL(gpio_touchcntrl_irq_get_value);
/****/

void gpio_cyttsp_wake_signal(void)
{
	mdelay(100);
	gpio_direction_output(MX6SL_PIN_TOUCH_INTB, 1);
	mdelay(1);
	gpio_set_value(MX6SL_PIN_TOUCH_INTB, 0);
	mdelay(1);
	gpio_set_value(MX6SL_PIN_TOUCH_INTB, 1);
	mdelay(1);
	gpio_set_value(MX6SL_PIN_TOUCH_INTB, 0);
	mdelay(1);
	gpio_set_value(MX6SL_PIN_TOUCH_INTB, 1);
	mdelay(100);
	gpio_direction_input(MX6SL_PIN_TOUCH_INTB);
}
EXPORT_SYMBOL(gpio_cyttsp_wake_signal);

/* power up cypress touch */
int gpio_cyttsp_hw_reset(void)
{

	/* greater than celeste 1.2 and icewine will have new gpio layout */
	gpio_direction_output(MX6SL_PIN_TOUCH_RST, 1);
	gpio_set_value(MX6SL_PIN_TOUCH_RST, 1);
	msleep(20);
	gpio_set_value(MX6SL_PIN_TOUCH_RST, 0);
	msleep(40);
	gpio_set_value(MX6SL_PIN_TOUCH_RST, 1);
	msleep(20);
	gpio_direction_input(MX6SL_PIN_TOUCH_RST);
	printk(KERN_DEBUG "cypress reset");

	return 0;
}
EXPORT_SYMBOL(gpio_cyttsp_hw_reset);


int gpio_setup_wdog(void)
{
	int ret;
        ret = gpio_request(MX6_WARIO_WDOG_B, "wario_wdog");
        if(unlikely(ret)) return ret;

	gpio_set_value(MX6_WARIO_WDOG_B, 1);
	gpio_direction_output(MX6_WARIO_WDOG_B, 1);
	return 0;
}
EXPORT_SYMBOL(gpio_setup_wdog);


// Function to config iomux for instance epdc.
void epdc_iomux_config_lve(void)
{
    // Config epdc.GDCLK to pad EPDC_GDCLK(A12)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_GDCLK(0x020E00D0)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_GDCLK));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_GDCLK(0x020E03C0)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_GDCLK));

    // Config epdc.GDOE to pad EPDC_GDOE(B13)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_GDOE(0x020E00D4)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_GDOE));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_GDOE(0x020E03C4)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_GDOE));

    // Config epdc.GDSP to pad EPDC_GDSP(A11)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_GDSP(0x020E00DC)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_GDSP));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_GDSP(0x020E03CC)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_GDSP));

    // Config epdc.PWRCOM to pad EPDC_PWRCOM(B11)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_PWRCOM(0x020E00E0)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_PWRCOM));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_PWRCOM(0x020E03D0)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_PWRCOM));

    // Config epdc.PWRSTAT to pad EPDC_PWRSTAT(E10)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_PWRSTAT(0x020E00F8)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT5 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_PWRSTAT));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_PWRSTAT(0x020E03E8)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PU & 0x3) << 14 |
           (PUE_PULL & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_ENABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_PWRSTAT));

    // Config epdc.SDCE[0] to pad EPDC_SDCE0(C11)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_SDCE0(0x020E0100)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_SDCE0));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_SDCE0(0x020E03F0)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_SLOW & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_SDCE0));

    // Config epdc.SDCLK to pad EPDC_SDCLK(B10)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_SDCLK(0x020E0110)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_SDCLK));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_SDCLK(0x020E0400)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_48OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_SDCLK));

    // Config epdc.SDDO[0] to pad EPDC_D0(A18)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D0(0x020E0090)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D0));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D0(0x020E0380)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D0));

    // Config epdc.SDDO[1] to pad EPDC_D1(A17)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D1(0x020E0094)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D1));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D1(0x020E0384)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D1));

    // Config epdc.SDDO[2] to pad EPDC_D2(B17)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D2(0x020E00B0)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D2));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D2(0x020E03A0)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D2));

    // Config epdc.SDDO[3] to pad EPDC_D3(A16)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D3(0x020E00B4)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D3));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D3(0x020E03A4)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D3));

    // Config epdc.SDDO[4] to pad EPDC_D4(B16)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D4(0x020E00B8)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D4));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D4(0x020E03A8)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D4));

    // Config epdc.SDDO[5] to pad EPDC_D5(A15)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D5(0x020E00BC)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D5));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D5(0x020E03AC)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D5));

    // Config epdc.SDDO[6] to pad EPDC_D6(B15)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D6(0x020E00C0)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D6));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D6(0x020E03B0)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D6));

    // Config epdc.SDDO[7] to pad EPDC_D7(C15)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_D7(0x020E00C4)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_D7));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_D7(0x020E03B4)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_D7));

    // Config epdc.SDLE to pad EPDC_SDLE(B8)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_SDLE(0x020E0114)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_SDLE));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_SDLE(0x020E0404)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_60OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_SDLE));

    // Config epdc.SDOE to pad EPDC_SDOE(E7)
    // Mux Register:
    // IOMUXC_SW_MUX_CTL_PAD_EPDC_SDOE(0x020E0118)
    __raw_writel((SION_DISABLED & 0x1) << 4 | (ALT0 & 0x7), (IOMUXC_SW_MUX_CTL_PAD_EPDC_SDOE));
    // Pad Control Register:
    // IOMUXC_SW_PAD_CTL_PAD_EPDC_SDOE(0x020E0408)
    __raw_writel((LVE_ENABLED & 0x1) << 22 | (HYS_ENABLED & 0x1) << 16 | (PUS_100KOHM_PD & 0x3) << 14 |
           (PUE_KEEP & 0x1) << 13 | (PKE_ENABLED & 0x1) << 12 | (ODE_DISABLED & 0x1) << 11 |
           (SPD_100MHZ & 0x3) << 6 | (DSE_48OHM & 0x7) << 3 | (SRE_FAST & 0x1), (IOMUXC_SW_PAD_CTL_PAD_EPDC_SDOE));
}
EXPORT_SYMBOL(epdc_iomux_config_lve);
/********************WAN GPIOs ********************/
void gpio_wan_ldo_fet_init(void)
{
	/* Note: gpio wan ldo/fet ctrl only needed for IW(WAN)EVT1.2 */
	if (lab126_board_is(BOARD_ID_ICEWINE_WARIO_EVT1_2)) {
		gpio_request(MX6SL_ARM2_EPDC_PWRCTRL3, "wan_ldo");
		gpio_direction_output(MX6SL_ARM2_EPDC_PWRCTRL3, 1);
		gpio_wan_ldo_en = 1;
	}
}
EXPORT_SYMBOL(gpio_wan_ldo_fet_init);

/* GPIO (LDO / FET) control to protect WAN module with 4.35V battery */
void gpio_wan_ldo_fet_ctrl(int enable)
{
	/* Note: gpio wan ldo/fet ctrl only needed for IW(WAN)EVT1.2 */
	if (lab126_board_is(BOARD_ID_ICEWINE_WARIO_EVT1_2)) {
		if (enable > 0) {
			if (!gpio_wan_ldo_en) {
				gpio_direction_output(MX6SL_ARM2_EPDC_PWRCTRL3, 1);		/* LDO */
				gpio_wan_ldo_en = 1;
			}
		} else {
			if (gpio_wan_ldo_en) {
				gpio_direction_output(MX6SL_ARM2_EPDC_PWRCTRL3, 0);		/* FET */
				gpio_wan_ldo_en = 0;
			}
		}
	}
}
EXPORT_SYMBOL(gpio_wan_ldo_fet_ctrl);

void wan_request_gpio(void)
{
	gpio_request(MX6_WAN_FW_READY, "FW_Ready");
	gpio_request(MX6_WAN_SHUTDOWN, "Power");
	gpio_request(MX6_WAN_ON_OFF, "Reset");
	gpio_request(MX6_WAN_USB_EN, "USBen");
	return;
}
EXPORT_SYMBOL(wan_request_gpio);

void wan_free_gpio(void)
{
	gpio_free(MX6_WAN_SHUTDOWN);
	gpio_free(MX6_WAN_ON_OFF);
	gpio_free(MX6_WAN_USB_EN);
	gpio_free(MX6_WAN_FW_READY);
	return;
}
EXPORT_SYMBOL(wan_free_gpio);

void gpio_wan_power(int enable)
{
	/* Set the direction to output */
	gpio_direction_output(MX6_WAN_SHUTDOWN, 0);

	/* Enable/disable power */
	gpio_set_value(MX6_WAN_SHUTDOWN, enable);
}
EXPORT_SYMBOL(gpio_wan_power);

void gpio_wan_rf_enable(int enable)
{
        gpio_direction_output(MX6_WAN_ON_OFF, 0);
        gpio_set_value(MX6_WAN_ON_OFF, enable);
}
EXPORT_SYMBOL(gpio_wan_rf_enable);

void gpio_wan_usb_enable(int enable)
{
        gpio_direction_output(MX6_WAN_USB_EN, 0);
        gpio_set_value(MX6_WAN_USB_EN, enable);
}
EXPORT_SYMBOL(gpio_wan_usb_enable);

int gpio_wan_mhi_irq(void)
{
        return gpio_to_irq(MX6_WAN_MHI);
}
EXPORT_SYMBOL(gpio_wan_mhi_irq);

#if 0
static void gpio_wan_hmi_irq(int enable)
{
        gpio_direction_output(MX6_WAN_HMI, 0);
        gpio_set_value(MX6_WAN_HMI, enable);
}
#endif

int gpio_wan_fw_ready_irq(void)
{
        return gpio_to_irq(MX6_WAN_FW_READY);
}
EXPORT_SYMBOL(gpio_wan_fw_ready_irq);

int gpio_wan_fw_ready(void)
{
        return gpio_get_value(MX6_WAN_FW_READY);
}
EXPORT_SYMBOL(gpio_wan_fw_ready);

# if 0 //PB: TODO
static void set_fw_ready_gpio_state(int enable)
{
        if( enable) {
                mxc_iomux_set_pad(MX50_PIN_EIM_DA6,
                        PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
                        PAD_CTL_ODE_OPENDRAIN_NONE |
                        PAD_CTL_100K_PU
                );
        } else {
                mxc_iomux_set_pad(MX50_PIN_EIM_DA6,
                        PAD_CTL_PKE_ENABLE | PAD_CTL_PUE_PULL |
                        PAD_CTL_ODE_OPENDRAIN_NONE |
                        PAD_CTL_100K_PD
                );
        }
}
#endif
int gpio_wan_usb_wake(void)
{
        return gpio_get_value(MX6_WAN_MHI);
}
EXPORT_SYMBOL(gpio_wan_usb_wake);

int gpio_wan_host_wake_irq(void)
{
	return gpio_to_irq(MX6_WAN_HOST_WAKE);
}
EXPORT_SYMBOL(gpio_wan_host_wake_irq);

int gpio_wan_host_wake(void)
{
	return gpio_get_value(MX6_WAN_HOST_WAKE);
}
EXPORT_SYMBOL(gpio_wan_host_wake);

/*
//EPDC_SDCE2 - SCL
//EPDC_SDCE3 - SDA
MX6SL_PAD_EPDC_SDCE2__GPIO_1_29
MX6SL_PAD_EPDC_SDCE3__GPIO_1_30
#define MX6SL_ARM2_EPDC_SDCE2           IMX_GPIO_NR(1, 29)
#define MX6SL_ARM2_EPDC_SDCE3           IMX_GPIO_NR(1, 30)
*/
/* gpio_i2c3_scl_toggle:
 *
 */
#define I2C_RESET_CLK_CNT      10
void gpio_i2c3_scl_toggle(void)
{
	int i = 0;
	gpio_direction_output(MX6SL_ARM2_EPDC_SDCE2, 1);
	gpio_set_value(MX6SL_ARM2_EPDC_SDCE2, 1);
	
	for (i = 0; i < I2C_RESET_CLK_CNT; i++) {
		gpio_set_value(MX6SL_ARM2_EPDC_SDCE2, 0);
		udelay(20);
		gpio_set_value(MX6SL_ARM2_EPDC_SDCE2, 1);
		udelay(20);
	}
} 
EXPORT_SYMBOL(gpio_i2c3_scl_toggle);

int gpio_i2c3_fault(void)
{
	int ret = 0;
	ret = (gpio_get_value(MX6SL_ARM2_EPDC_SDCE2) && 
			!gpio_get_value(MX6SL_ARM2_EPDC_SDCE3));
	return ret;

}
EXPORT_SYMBOL(gpio_i2c3_fault);

void config_i2c3_gpio_input(int enable)
{
	if (enable) {
		gpio_request(MX6SL_ARM2_EPDC_SDCE2, "i2c3_scl");
		gpio_request(MX6SL_ARM2_EPDC_SDCE3, "i2c3_sda");
		mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE2__GPIO_1_29);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE3__GPIO_1_30);
	} else {
		gpio_free(MX6SL_ARM2_EPDC_SDCE2);
		gpio_free(MX6SL_ARM2_EPDC_SDCE3);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE2__I2C3_SCL);
		mxc_iomux_v3_setup_pad(MX6SL_PAD_EPDC_SDCE3__I2C3_SDA);
	}
}
EXPORT_SYMBOL(config_i2c3_gpio_input);
