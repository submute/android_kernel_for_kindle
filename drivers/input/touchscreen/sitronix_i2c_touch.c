/*
 * drivers/input/touchscreen/sitronix_i2c_touch.c
 *
 * Touchscreen driver for Sitronix (I2C bus)
 *
 * Copyright (C) 2011 Sitronix Technology Co., Ltd.
 *	Rudy Huang <rudy_huang@sitronix.com.tw>
 */
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/module.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif // CONFIG_HAS_EARLYSUSPEND
#include <linux/input/sitronix_i2c_touch.h>
#ifdef SITRONIX_FW_UPGRADE_FEATURE
#include <linux/cdev.h>
#include <asm/uaccess.h>
#endif // SITRONIX_FW_UPGRADE_FEATURE
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h> // to be compatible with linux kernel 3.2.15
#include <linux/gpio.h>
#ifdef SITRONIX_MONITOR_THREAD
#include <linux/kthread.h>
//#include <mach/gpio.h>
#endif // SITRONIX_MONITOR_THREAD

#define DRIVER_AUTHOR           "Sitronix, Inc."
#define DRIVER_NAME             "sitronix"
#define DRIVER_DESC             "Sitronix I2C touch"
#define DRIVER_DATE             "20120507"
#define DRIVER_MAJOR            2
#define DRIVER_MINOR         	9
#define DRIVER_PATCHLEVEL       1

MODULE_AUTHOR("Rudy Huang <rudy_huang@sitronix.com.tw>");
MODULE_DESCRIPTION("Sitronix I2C multitouch panels");
MODULE_LICENSE("GPL");

#ifdef SITRONIX_SENSOR_KEY
#define SITRONIX_NUMBER_SENSOR_KEY 3
int sitronix_sensor_key[SITRONIX_NUMBER_SENSOR_KEY] = {
	KEY_BACK, // bit 0
	KEY_HOME, // bit 1
	KEY_MENU, // bit 2
};
#endif // SITRONIX_SENSOR_KEY

#ifdef SITRONIX_TOUCH_KEY
#define SITRONIX_NUMBER_TOUCH_KEY 4

#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#define SITRONIX_TOUCH_RESOLUTION_X 480 /* max of X value in display area */
#define SITRONIX_TOUCH_RESOLUTION_Y 854 /* max of Y value in display area */
#define SITRONIX_TOUCH_GAP_Y	10  /* Gap between bottom of display and top of touch key */
#define SITRONIX_TOUCH_MAX_Y 915  /* resolution of y axis of touch ic */
struct sitronix_AA_key sitronix_key_array[SITRONIX_NUMBER_TOUCH_KEY] = {
	{15, 105, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_MENU}, /* MENU */
	{135, 225, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_HOME},
	{255, 345, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_BACK}, /* KEY_EXIT */
	{375, 465, SITRONIX_TOUCH_RESOLUTION_Y + SITRONIX_TOUCH_GAP_Y, SITRONIX_TOUCH_MAX_Y, KEY_SEARCH},
};
#else
#define SCALE_KEY_HIGH_Y 15
struct sitronix_AA_key sitronix_key_array[SITRONIX_NUMBER_TOUCH_KEY] = {
	{0, 0, 0, 0, KEY_MENU}, /* MENU */
	{0, 0, 0, 0, KEY_HOME},
	{0, 0, 0, 0, KEY_BACK}, /* KEY_EXIT */
	{0, 0, 0, 0, KEY_SEARCH},
};
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#endif // SITRONIX_TOUCH_KEY
struct sitronix_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	struct input_dev *keyevent_input;
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	int use_irq;
	struct hrtimer timer;
#ifndef SITRONIX_INT_POLLING_MODE
	struct work_struct  work;
#else
	struct delayed_work work;
#endif // SITRONIX_INT_POLLING_MODE
	void (*reset_ic)(void);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif // CONFIG_HAS_EARLYSUSPEND
	uint8_t fw_revision[4];
	int resolution_x;
	int resolution_y;
	uint8_t max_touches;
	uint8_t touch_protocol_type;
	uint8_t pixel_length;
	int suspend_state;
};

static int i2cErrorCount = 0;

#ifdef SITRONIX_MONITOR_THREAD
static struct task_struct * SitronixMonitorThread = NULL;
static int gMonitorThreadSleepInterval = 300; // 0.3 sec
static atomic_t iMonitorThreadPostpone = ATOMIC_INIT(0);

static uint8_t PreCheckData[4] ;
static int StatusCheckCount = 0;
static int sitronix_ts_monitor_thread(void *data);
static int sitronix_ts_delay_monitor_thread_start = DELAY_MONITOR_THREAD_START_PROBE; 
#endif // SITRONIX_MONITOR_THREAD

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sitronix_ts_early_suspend(struct early_suspend *h);
static void sitronix_ts_late_resume(struct early_suspend *h);
#endif // CONFIG_HAS_EARLYSUSPEND

static MTD_STRUCTURE sitronix_ts_gMTDPreStructure[SITRONIX_MAX_SUPPORTED_POINT]={{0}};

static struct sitronix_ts_data *sitronix_ts_gpts = NULL;
static int sitronix_ts_irq_on = 0;

#ifdef SITRONIX_FW_UPGRADE_FEATURE
int      sitronix_release(struct inode *, struct file *);
int      sitronix_open(struct inode *, struct file *);
ssize_t  sitronix_write(struct file *file, const char *buf, size_t count, loff_t *ppos);
ssize_t  sitronix_read(struct file *file, char *buf, size_t count, loff_t *ppos);
long	 sitronix_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static struct cdev sitronix_cdev;
static struct class *sitronix_class;
static int sitronix_major = 0;

int  sitronix_open(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(sitronix_open);

int  sitronix_release(struct inode *inode, struct file *filp)
{
	return 0;
}
EXPORT_SYMBOL(sitronix_release);

ssize_t  sitronix_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int ret;
	char *tmp;

	if (count > 8192)
		count = 8192;

	tmp = (char *)kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;
	if (copy_from_user(tmp,buf,count)) {
		kfree(tmp);
		return -EFAULT;
	}
	UpgradeMsg("writing %zu bytes.\n", count);

	ret = i2c_master_send(sitronix_ts_gpts->client, tmp, count);
	kfree(tmp);
	return ret;
}
EXPORT_SYMBOL(sitronix_write);

ssize_t  sitronix_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	char *tmp;
	int ret;

	if (count > 8192)
		count = 8192;

	tmp = (char *)kmalloc(count,GFP_KERNEL);
	if (tmp==NULL)
		return -ENOMEM;

	UpgradeMsg("reading %zu bytes.\n", count);

	ret = i2c_master_recv(sitronix_ts_gpts->client, tmp, count);
	if (ret >= 0)
		ret = copy_to_user(buf,tmp,count)?-EFAULT:ret;
	kfree(tmp);
	return ret;
}
EXPORT_SYMBOL(sitronix_read);

static int sitronix_ts_resume(struct i2c_client *client);
static int sitronix_ts_suspend(struct i2c_client *client, pm_message_t mesg);
long	 sitronix_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	uint8_t temp[4];

	if (_IOC_TYPE(cmd) != SMT_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SMT_IOC_MAXNR) return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,(void __user *)arg,\
				 _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ,(void __user *)arg,\
				  _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
		case IOCTL_SMT_GET_DRIVER_REVISION:
			UpgradeMsg("IOCTL_SMT_GET_DRIVER_REVISION\n");
			temp[0] = SITRONIX_TOUCH_DRIVER_VERSION;
			if(copy_to_user((uint8_t __user *)arg, &temp[0], 1)){
				UpgradeMsg("fail to get driver version\n");
				retval = -EFAULT;
			}
			break;
		case IOCTL_SMT_GET_FW_REVISION:
			UpgradeMsg("IOCTL_SMT_GET_FW_REVISION\n");
			if(copy_to_user((uint8_t __user *)arg, &sitronix_ts_gpts->fw_revision[0], 4))
					retval = -EFAULT;
			break;
		case IOCTL_SMT_ENABLE_IRQ:
			UpgradeMsg("IOCTL_SMT_ENABLE_IRQ\n");
			if(!sitronix_ts_irq_on){
				sitronix_ts_irq_on = 1;
				enable_irq(sitronix_ts_gpts->client->irq);
#ifdef SITRONIX_MONITOR_THREAD
				atomic_set(&iMonitorThreadPostpone,1);
				SitronixMonitorThread = kthread_run(sitronix_ts_monitor_thread,"Sitronix","Monitorthread");
				if(IS_ERR(SitronixMonitorThread))
					SitronixMonitorThread = NULL;
#endif // SITRONIX_MONITOR_THREAD
			}
			break;
		case IOCTL_SMT_DISABLE_IRQ:
			UpgradeMsg("IOCTL_SMT_DISABLE_IRQ\n");
			if(sitronix_ts_irq_on){
				sitronix_ts_irq_on = 0;
				disable_irq_nosync(sitronix_ts_gpts->client->irq);
#ifdef SITRONIX_MONITOR_THREAD
				if(SitronixMonitorThread){
					kthread_stop(SitronixMonitorThread);
					SitronixMonitorThread = NULL;
				}
#endif // SITRONIX_MONITOR_THREAD
			}
			break;
		case IOCTL_SMT_RESUME:
			UpgradeMsg("IOCTL_SMT_RESUME\n");
			sitronix_ts_resume(sitronix_ts_gpts->client);
			break;
		case IOCTL_SMT_SUSPEND:
			UpgradeMsg("IOCTL_SMT_SUSPEND\n");
			sitronix_ts_suspend(sitronix_ts_gpts->client, PMSG_SUSPEND);
			break;
		case IOCTL_SMT_HW_RESET:
			UpgradeMsg("IOCTL_SMT_HW_RESET\n");
			if(sitronix_ts_gpts->reset_ic)
				sitronix_ts_gpts->reset_ic();
			break;
		default:
			retval = -ENOTTY;
	}

	return retval;
}
EXPORT_SYMBOL(sitronix_ioctl);
#endif // SITRONIX_FW_UPGRADE_FEATURE
static int sitronix_get_fw_revision(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[4];

	buffer[0] = FIRMWARE_REVISION_3;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send fw revision command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 4);
	if (ret < 0){
		printk("read fw revision error (%d)\n", ret);
		return ret;
	}else{
		memcpy(ts->fw_revision, buffer, 4);
		printk("fw revision (hex) = %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}
	return 0;
}
static int sitronix_get_max_touches(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[1];

	buffer[0] = MAX_NUM_TOUCHES;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send max touches command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read max touches error (%d)\n", ret);
		return ret;
	}else{
		ts->max_touches = buffer[0];
		printk("max touches = %d \n",ts->max_touches);
	}
	return 0;
}

static int sitronix_get_protocol_type(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[1];

	buffer[0] = I2C_PROTOCOL;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send i2c protocol command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read i2c protocol error (%d)\n", ret);
		return ret;
	}else{
		ts->touch_protocol_type = buffer[0] & I2C_PROTOCOL_BMSK;
		if(ts->touch_protocol_type == SITRONIX_A_TYPE)
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		else if(ts->touch_protocol_type == SITRONIX_B_TYPE)
			ts->pixel_length = PIXEL_DATA_LENGTH_B;
		else
			ts->pixel_length = PIXEL_DATA_LENGTH_A;
		printk("i2c protocol = %d \n", ts->touch_protocol_type);
	}
	return 0;
}

static int sitronix_get_resolution(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t buffer[3];

	buffer[0] = XY_RESOLUTION_HIGH;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send resolution command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 3);
	if (ret < 0){
		printk("read resolution error (%d)\n", ret);
		return ret;
	}else{
		ts->resolution_x = ((buffer[0] & (X_RES_H_BMSK << X_RES_H_SHFT)) << 4) | buffer[1];
		ts->resolution_y = ((buffer[0] & Y_RES_H_BMSK) << 8) | buffer[2];
		printk("resolution = %d x %d\n", ts->resolution_x, ts->resolution_y);
	}
	return 0;
}

static int sitronix_ts_set_powerdown_bit(struct sitronix_ts_data *ts, int value)
{
	int ret = 0;
	uint8_t buffer[2];

	DbgMsg("%s, value = %d\n", __FUNCTION__, value);
	buffer[0] = DEVICE_CONTROL_REG;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send device control command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read device control status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("dev status = %d \n", buffer[0]);
	}

	if(value == 0)
		buffer[1] = buffer[0] & 0xfd;
	else
		buffer[1] = buffer[0] | 0x2;

	buffer[0] = DEVICE_CONTROL_REG;
	ret = i2c_master_send(ts->client, buffer, 2);
	if (ret < 0){
		printk("write power down error (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int sitronix_ts_get_touch_info(struct sitronix_ts_data *ts)
{
	int ret = 0;
	ret = sitronix_get_resolution(ts);
	if(ret < 0)
		return ret;
	ret = sitronix_get_fw_revision(ts);
	if(ret < 0)
		return ret;
	if((ts->fw_revision[0] == 0) && (ts->fw_revision[1] == 0)){
		ts->touch_protocol_type = SITRONIX_B_TYPE;
		ts->pixel_length = PIXEL_DATA_LENGTH_B;
		ts->max_touches = 2;
		printk("i2c protocol = %d \n", ts->touch_protocol_type);
		printk("max touches = %d \n",ts->max_touches);
	}else{
		ret = sitronix_get_protocol_type(ts);
		if(ret < 0)
			return ret;
		if(ts->touch_protocol_type == SITRONIX_B_TYPE){
			ts->max_touches = 2;
			printk("max touches = %d \n",ts->max_touches);
		}else{
			ret = sitronix_get_max_touches(ts);
			if(ret < 0)
				return ret;
		}
	}
	return 0;
}

static int sitronix_ts_get_device_status(struct sitronix_ts_data *ts, uint8_t *dev_status)
{
	int ret = 0;
	uint8_t buffer[8];

	DbgMsg("%s\n", __FUNCTION__);
	buffer[0] = STATUS_REG;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send status reg command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 8);
	if (ret < 0){
		printk("read status reg error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("status reg = %d \n", buffer[0]);
	}

	*dev_status = buffer[0] & 0xf;

	return 0;
}

static int sitronix_ts_Enhance_Function_control(struct sitronix_ts_data *ts, uint8_t *value)
{
	int ret = 0;
	uint8_t buffer[4];

	DbgMsg("%s\n", __FUNCTION__);
	buffer[0] = 0xF0;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send Enhance Function command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read Enhance Functions status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("Enhance Functions status = %d \n", buffer[0]);
	}

	*value = buffer[0] & 0x4;

	return 0;
}

static int sitronix_ts_FW_Bank_Select(struct sitronix_ts_data *ts, uint8_t value)
{
	int ret = 0;
	uint8_t buffer[1];

	DbgMsg("%s\n", __FUNCTION__);
	buffer[0] = 0xF1;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send FW Bank Select command error (%d)\n", ret);
		return ret;
	}

	ret = i2c_master_recv(ts->client, buffer, 1);
	if (ret < 0){
		printk("read FW Bank Select status error (%d)\n", ret);
		return ret;
	}else{
		DbgMsg("FW Bank Select status = %d \n", buffer[0]);
	}

	buffer[1] = ((buffer[0] & 0xfc) | value);
	buffer[0] = 0xF1;
	ret = i2c_master_send(ts->client, buffer, 2);
	if (ret < 0){
		printk("send FW Bank Select command error (%d)\n", ret);
		return ret;
	}

	return 0;
}

static int sitronix_get_id_info(struct sitronix_ts_data *ts, uint8_t *id_info)
{
	int ret = 0;
	uint8_t buffer[4];

	buffer[0] = 0x0C;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0){
		printk("send id info command error (%d)\n", ret);
		return ret;
	}
	ret = i2c_master_recv(ts->client, buffer, 4);
	if (ret < 0){
		printk("read id info error (%d)\n", ret);
		return ret;
	}else{
		memcpy(id_info, buffer, 4);
	}
	return 0;
}

static int sitronix_ts_identify(struct sitronix_ts_data *ts)
{
	int ret = 0;
	uint8_t id[4];
	uint8_t Enhance_Function = 0;

	ret = sitronix_ts_FW_Bank_Select(ts, 1);
	if(ret < 0)
		return ret;
	ret = sitronix_ts_Enhance_Function_control(ts, &Enhance_Function);
	if(ret < 0)
		return ret;
	if(Enhance_Function == 0x4){
		ret = sitronix_get_id_info(ts, &id[0]);
		if(ret < 0)
			return ret;
		printk("id (hex) = %x %x %x %x\n", id[0], id[1], id[2], id[3]);
		if((id[0] == 1)&&(id[1] == 2)&&(id[2] == 0xb)&&(id[3] == 1)){
			return 0;
		}else{
			printk("Error: It is not Sitronix IC\n");
			return -1;
		}
	}else{
		printk("Error: Can not get ID of Sitronix IC\n");
		return -1;
	}
}

#ifdef SITRONIX_MONITOR_THREAD
static int sitronix_ts_monitor_thread(void *data)
{
	int ret = 0;
	uint8_t buffer[4];
	int result = 0;
	int once = 1;
	DbgMsg("%s:\n", __FUNCTION__);

	printk("delay %d ms\n", sitronix_ts_delay_monitor_thread_start);
	msleep(sitronix_ts_delay_monitor_thread_start);
	while(!kthread_should_stop()){
		DbgMsg("%s:\n", "Sitronix_ts_monitoring");
		if(atomic_read(&iMonitorThreadPostpone)){
		 		atomic_set(&iMonitorThreadPostpone,0);
		}else{
			if(once == 1){
				buffer[0] = DEVICE_CONTROL_REG;
				ret = i2c_master_send(sitronix_ts_gpts->client, buffer, 1);
				if (ret < 0){
					DbgMsg("send device control command error (%d)\n", ret);
					goto exit_i2c_invalid;
				}
				ret = i2c_master_recv(sitronix_ts_gpts->client, buffer, 1);
				if (ret < 0){
					DbgMsg("read device control status error (%d)\n", ret);
					goto exit_i2c_invalid;
				}else{
					DbgMsg("read DEVICE_CONTROL_REG status = %d \n", buffer[0]);
				}
				buffer[0] &= 0xf3;
				ret = i2c_master_send(sitronix_ts_gpts->client, buffer, 1);
				if (ret < 0){
					DbgMsg("write power down error (%d)\n", ret);
					goto exit_i2c_invalid;
				}
				once = 0;
			}
			buffer[0] = 0x40;
			ret = i2c_master_send(sitronix_ts_gpts->client, buffer, 1);
			if (ret < 0){
				DbgMsg("send device control command error (%d)\n", ret);
				goto exit_i2c_invalid;
			}
			ret = i2c_master_recv(sitronix_ts_gpts->client, buffer, 4);
			if (ret < 0){
				DbgMsg("read device 1D data error (%d)\n", ret);
				goto exit_i2c_invalid;
			}else{
				DbgMsg("1D data h40-43 = %d, %d, %d, %d \n", buffer[0], buffer[1], buffer[2], buffer[3]);
				result = 1;
				if ((PreCheckData[0] == buffer[0]) && (PreCheckData[1] == buffer[1]) && 
				(PreCheckData[2] == buffer[2]) && (PreCheckData[3] == buffer[3]))
					StatusCheckCount ++;
				else
					StatusCheckCount =0;
				PreCheckData[0] = buffer[0];
				PreCheckData[1] = buffer[1];
				PreCheckData[2] = buffer[2];
				PreCheckData[3] = buffer[3];
				if (3 <= StatusCheckCount){
					DbgMsg("IC Status doesn't update! \n");
					result = -1;
					StatusCheckCount = 0;
				}
			}
			if (-1 == result){
				printk("Chip abnormal, reset it!\n");
				if(sitronix_ts_gpts->reset_ic)
					sitronix_ts_gpts->reset_ic();
		   		i2cErrorCount = 0;
		   		StatusCheckCount = 0;
		    	}
exit_i2c_invalid:
			if(0 == result){
				i2cErrorCount ++;
				if ((2 <= i2cErrorCount)){
					printk("I2C abnormal, reset it!\n");
					if(sitronix_ts_gpts->reset_ic)
						sitronix_ts_gpts->reset_ic();
		    			i2cErrorCount = 0;
		    			StatusCheckCount = 0;
		    		}
		    	}else
		    		i2cErrorCount = 0;
		}
		msleep(gMonitorThreadSleepInterval);
	}
	DbgMsg("%s exit\n", __FUNCTION__);
	return 0;
}
#endif // SITRONIX_MONITOR_THREAD

static void sitronix_ts_work_func(struct work_struct *work)
{
	int i;
#ifdef SITRONIX_TOUCH_KEY
	int j;
#endif // SITRONIX_TOUCH_KEY
	int ret;
#ifndef SITRONIX_INT_POLLING_MODE
	struct sitronix_ts_data *ts = container_of(work, struct sitronix_ts_data, work);
#else
	struct sitronix_ts_data *ts = container_of(to_delayed_work(work), struct sitronix_ts_data, work);
#endif // SITRONIX_INT_POLLING_MODE
	uint8_t buffer[2+ SITRONIX_MAX_SUPPORTED_POINT * PIXEL_DATA_LENGTH_A];
	static MTD_STRUCTURE MTDStructure[SITRONIX_MAX_SUPPORTED_POINT]={{0}};
	uint8_t PixelCount = 0;
	static uint8_t all_clear = 1;

	DbgMsg("%s\n",  __FUNCTION__);
	if(ts->suspend_state){
		goto exit_invalid_data;
	}

	// get finger count
	buffer[0] = FINGERS;
	ret = i2c_master_send(ts->client, buffer, 1);
	if (ret < 0)
		printk("send finger command error (%d)\n", ret);
	ret = i2c_master_recv(ts->client, buffer, 2 + ts->max_touches * ts->pixel_length);
	if (ret < 0) {
		printk("read finger error (%d)\n", ret);
		i2cErrorCount ++;
		goto exit_invalid_data;
	}else{
		i2cErrorCount = 0;
#ifdef SITRONIX_FINGER_COUNT_REG_ENABLE
		PixelCount = buffer[0] & FINGERS_BMSK ;
#else
		for(i = 0; i < ts->max_touches; i++){
			if(buffer[2 + i * ts->pixel_length] >= 0x80)
				PixelCount++;
		}
#endif // SITRONIX_FINGER_COUNT_REG_ENABLE
		DbgMsg("fingers = %d\n", PixelCount);
	}

#ifdef SITRONIX_SENSOR_KEY
	for(i = 0; i < SITRONIX_NUMBER_SENSOR_KEY; i++){
		if(buffer[1] & (1 << i)){
			DbgMsg("key[%d] down\n", i);
			input_report_key(ts->keyevent_input, sitronix_sensor_key[i], 1);
		}else{
			DbgMsg("key[%d] up\n", i);
			input_report_key(ts->keyevent_input, sitronix_sensor_key[i], 0);
		}
	}
#endif // SITRONIX_SENSOR_KEY

	for(i = 0; i < ts->max_touches; i++){
#ifndef SITRONIX_TOUCH_KEY
		if((buffer[2 + ts->pixel_length * i] >> X_COORD_VALID_SHFT) == 1){
			MTDStructure[i].Pixel_X = ((buffer[2 + ts->pixel_length * i] & (X_COORD_H_BMSK << X_COORD_H_SHFT)) << 4) |  (buffer[2 + ts->pixel_length * i + X_COORD_L]);
			MTDStructure[i].Pixel_Y = ((buffer[2 + ts->pixel_length * i] & Y_COORD_H_BMSK) << 8) |  (buffer[2 + ts->pixel_length * i + Y_COORD_L]);
			MTDStructure[i].Current_Pressed_area = AREA_DISPLAY;
		}else
			MTDStructure[i].Current_Pressed_area = AREA_NONE;
#else
		MTDStructure[i].Pixel_X = ((buffer[2 + ts->pixel_length * i] & (X_COORD_H_BMSK << X_COORD_H_SHFT)) << 4) |  (buffer[2 + ts->pixel_length * i + X_COORD_L]);
		MTDStructure[i].Pixel_Y = ((buffer[2 + ts->pixel_length * i] & Y_COORD_H_BMSK) << 8) |  (buffer[2 + ts->pixel_length * i + Y_COORD_L]);
		//DbgMsg("[%d] (%d, %d)\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
		if((buffer[2 + ts->pixel_length * i] >> X_COORD_VALID_SHFT) == 1){
			MTDStructure[i].Current_Pressed_area = AREA_INVALID;
#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
			if((MTDStructure[i].Pixel_X < ts->resolution_x) && (MTDStructure[i].Pixel_Y < sitronix_key_array[0].y_low)){
#else
			if((MTDStructure[i].Pixel_X < ts->resolution_x) && (MTDStructure[i].Pixel_Y < (ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y))){
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
				MTDStructure[i].Current_Pressed_area = AREA_DISPLAY;
				//DbgMsg("AREA_DISPLAY\n");
			}else{
				for(j = 0; j < SITRONIX_NUMBER_TOUCH_KEY; j++){
					if((MTDStructure[i].Pixel_X >= sitronix_key_array[j].x_low) &&
					(MTDStructure[i].Pixel_X <= sitronix_key_array[j].x_high) &&
					(MTDStructure[i].Pixel_Y >= sitronix_key_array[j].y_low) &&
					(MTDStructure[i].Pixel_Y <= sitronix_key_array[j].y_high)){
						MTDStructure[i].Current_Pressed_area = AREA_KEY;
						MTDStructure[i].Current_key_index = j;
						//DbgMsg("AREA_KEY [%d]\n", j);
						break;
					}
				}
			}
		}else{
			MTDStructure[i].Current_Pressed_area = AREA_NONE;
			//DbgMsg("AREA_NONE\n");
		}
#endif // SITRONIX_TOUCH_KEY

	}
	if(PixelCount != 0){
	for(i = 0; i < ts->max_touches; i++){
#ifndef SITRONIX_TOUCH_KEY
		input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
		if(MTDStructure[i].Current_Pressed_area == AREA_DISPLAY){
			input_report_abs(ts->input_dev,  ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
			input_report_abs(ts->input_dev,  ABS_MT_POSITION_Y, (ts->resolution_y - MTDStructure[i].Pixel_Y));
			input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
			input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
			input_report_abs(ts->input_dev,  ABS_MT_PRESSURE, 255);
			if(i == 0) input_report_key(ts->input_dev,  BTN_TOUCH, 1);
			DbgMsg("[%d](%d, %d)+\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
			input_mt_sync(ts->input_dev);
		//}else if((MTDStructure[i].Current_Pressed_area == AREA_NONE)&&(sitronix_ts_gMTDPreStructure[i].Current_Pressed_area == AREA_DISPLAY)){
		}else if(MTDStructure[i].Current_Pressed_area == AREA_NONE){
			input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(ts->input_dev,  ABS_MT_PRESSURE, 0);
			if(i == 0) input_report_key(ts->input_dev,  BTN_TOUCH, 0);
			DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
			input_mt_sync(ts->input_dev);
		}
		memcpy(&sitronix_ts_gMTDPreStructure[i], &MTDStructure[i], sizeof(MTD_STRUCTURE));
#else
		//DbgMsg("[%d] %d => %d\n", i, sitronix_ts_gMTDPreStructure[i].First_Pressed_area, MTDStructure[i].Current_Pressed_area);
		if(sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_NONE){
			if(MTDStructure[i].Current_Pressed_area == AREA_DISPLAY){
				input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input_dev,  ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
				input_report_abs(ts->input_dev,  ABS_MT_POSITION_Y, MTDStructure[i].Pixel_Y);
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
				input_mt_sync(ts->input_dev);
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_DISPLAY;
				DbgMsg("[%d](%d, %d)\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
			}else if(MTDStructure[i].Current_Pressed_area == AREA_KEY){
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_KEY;
				sitronix_ts_gMTDPreStructure[i].First_key_index = MTDStructure[i].Current_key_index;
				input_report_key(ts->keyevent_input, sitronix_key_array[MTDStructure[i].Current_key_index].code, 1);
				DbgMsg("key [%d] down\n", MTDStructure[i].Current_key_index);
			}
		}else if(sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_DISPLAY){
			if(MTDStructure[i].Current_Pressed_area == AREA_DISPLAY){
				input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input_dev,  ABS_MT_POSITION_X, MTDStructure[i].Pixel_X);
				input_report_abs(ts->input_dev,  ABS_MT_POSITION_Y, MTDStructure[i].Pixel_Y);
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 1);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 1);
				input_mt_sync(ts->input_dev);
				DbgMsg("[%d](%d, %d)+\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
			}else if(MTDStructure[i].Current_Pressed_area == AREA_KEY){
				input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
				input_mt_sync(ts->input_dev);
				DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
			}else if(MTDStructure[i].Current_Pressed_area == AREA_NONE){
				input_report_abs(ts->input_dev,  ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
				input_mt_sync(ts->input_dev);
				DbgMsg("[%d](%d, %d)-\n", i, MTDStructure[i].Pixel_X, MTDStructure[i].Pixel_Y);
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
			}
		}else if(sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_KEY){
			if(MTDStructure[i].Current_Pressed_area == AREA_KEY){
				input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 1);
				DbgMsg("key [%d] down+\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
			}else if(MTDStructure[i].Current_Pressed_area == AREA_DISPLAY){
				input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
				DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
			}else if(MTDStructure[i].Current_Pressed_area == AREA_NONE){
				input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
				DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
				sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
			}
		}
#endif // SITRONIX_TOUCH_KEY
	}
		all_clear = 0;
#ifdef SITRONIX_INT_POLLING_MODE
#ifdef SITRONIX_MONITOR_THREAD
		atomic_set(&iMonitorThreadPostpone,1);
#endif // SITRONIX_MONITOR_THREAD
		schedule_delayed_work(&ts->work, msecs_to_jiffies(INT_POLLING_MODE_INTERVAL));
#endif // SITRONIX_INT_POLLING_MODE
	}else{
		if(all_clear == 0){
			input_report_abs(ts->input_dev,  ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(ts->input_dev,  ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(ts->input_dev,  ABS_MT_PRESSURE, 0);
			input_report_key(ts->input_dev,  BTN_TOUCH, 0);
			input_mt_sync(ts->input_dev);
			all_clear = 1;
#ifdef SITRONIX_TOUCH_KEY
			for(i = 0; i < ts->max_touches; i++){
				if(sitronix_ts_gMTDPreStructure[i].First_Pressed_area == AREA_KEY){
					input_report_key(ts->keyevent_input, sitronix_key_array[sitronix_ts_gMTDPreStructure[i].First_key_index].code, 0);
					DbgMsg("key [%d] up\n", sitronix_ts_gMTDPreStructure[i].First_key_index);
					sitronix_ts_gMTDPreStructure[i].First_Pressed_area = AREA_NONE;
				}
			}
#endif // SITRONIX_TOUCH_KEY
		}else{
			DbgMsg("ignore dummy finger leave\n");
		}
#ifdef SITRONIX_INT_POLLING_MODE
		if (ts->use_irq){
			sitronix_ts_irq_on = 1;
			enable_irq(ts->client->irq);
		}
#endif // SITRONIX_INT_POLLING_MODE
	}
	input_sync(ts->input_dev);

exit_invalid_data:
#if defined(SITRONIX_LEVEL_TRIGGERED)
	if (ts->use_irq){
		sitronix_ts_irq_on = 1;
		enable_irq(ts->client->irq);
	}
#endif // defined(SITRONIX_LEVEL_TRIGGERED)
	if ((2 <= i2cErrorCount)){
		printk("I2C abnormal in work_func(), reset it!\n");
		if(sitronix_ts_gpts->reset_ic)
			sitronix_ts_gpts->reset_ic();
   		i2cErrorCount = 0;
#ifdef SITRONIX_MONITOR_THREAD
   		StatusCheckCount = 0;
#endif // SITRONIX_MONITOR_THREAD
	}
;
}


static irqreturn_t sitronix_ts_irq_handler(int irq, void *dev_id)
{
	struct sitronix_ts_data *ts = dev_id;

	DbgMsg("%s\n", __FUNCTION__);
#if defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
	sitronix_ts_irq_on = 0;
	disable_irq_nosync(ts->client->irq);
#endif // defined(SITRONIX_LEVEL_TRIGGERED) || defined(SITRONIX_INT_POLLING_MODE)
#ifdef SITRONIX_MONITOR_THREAD
	atomic_set(&iMonitorThreadPostpone,1);
#endif // SITRONIX_MONITOR_THREAD
#ifndef SITRONIX_INT_POLLING_MODE
	schedule_work(&ts->work);
#else	
	schedule_delayed_work(&ts->work, msecs_to_jiffies(0));
#endif // SITRONIX_INT_POLLING_MODE
	return IRQ_HANDLED;
}

static int sitronix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	int i;
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	struct sitronix_ts_data *ts;
	int ret = 0;
	uint16_t max_x = 0, max_y = 0;
	struct sitronix_i2c_touch_platform_data *pdata;
	uint8_t dev_status = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
#ifndef SITRONIX_INT_POLLING_MODE
	INIT_WORK(&ts->work, sitronix_ts_work_func);
#else	
	INIT_DELAYED_WORK(&ts->work, sitronix_ts_work_func);
#endif // SITRONIX_INT_POLLING_MODE
	ts->client = client;
	i2c_set_clientdata(client, ts);
	pdata = client->dev.platform_data;

	if(pdata->reset_ic){
		ts->reset_ic = pdata->reset_ic;
		pdata->reset_ic();
		mdelay(SITRONIX_TS_CHANGE_MODE_DELAY);
	}
	sitronix_ts_gpts = ts;

	ret = sitronix_ts_get_device_status(ts, &dev_status);

	if((ret < 0) || (dev_status == 0x6))
		goto err_device_info_error;

	ret = sitronix_ts_get_touch_info(ts);
	if(ret < 0)
		goto err_device_info_error;

	//ret = sitronix_ts_identify(ts);
	//if(ret < 0)
	//	goto err_device_info_error;

#ifdef SITRONIX_MONITOR_THREAD
	//== Add thread to monitor chip
	atomic_set(&iMonitorThreadPostpone,1);
	SitronixMonitorThread = kthread_run(sitronix_ts_monitor_thread,"Sitronix","Monitorthread");
	if(IS_ERR(SitronixMonitorThread))
		SitronixMonitorThread = NULL;
#endif // SITRONIX_MONITOR_THREAD

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL){
		printk("Can not allocate memory for input device.");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}

	ts->input_dev->name = "sitronix-i2c-touch-mt";
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);

#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ts->keyevent_input = input_allocate_device();
	if (ts->keyevent_input == NULL){
		printk("Can not allocate memory for key input device.");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	}
	ts->keyevent_input->name  = "sitronix-i2c-touch-key";
	set_bit(EV_KEY, ts->keyevent_input->evbit);
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
#if defined(SITRONIX_SENSOR_KEY)
	for(i = 0; i < SITRONIX_NUMBER_SENSOR_KEY; i++){
		set_bit(sitronix_sensor_key[i], ts->keyevent_input->keybit);
	}
#endif // defined(SITRONIX_SENSOR_KEY)

#ifndef SITRONIX_TOUCH_KEY
	max_x = ts->resolution_x;
	max_y = ts->resolution_y;
#else
#ifdef SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
	for(i = 0; i < SITRONIX_NUMBER_TOUCH_KEY; i++){
		set_bit(sitronix_key_array[i].code, ts->keyevent_input->keybit);
	}
	max_x = SITRONIX_TOUCH_RESOLUTION_X;
	max_y = SITRONIX_TOUCH_RESOLUTION_Y;
#else
	for(i = 0; i < SITRONIX_NUMBER_TOUCH_KEY; i++){
		sitronix_key_array[i].x_low = ((ts->resolution_x / SITRONIX_NUMBER_TOUCH_KEY ) * i ) + 15;
		sitronix_key_array[i].x_high = ((ts->resolution_x / SITRONIX_NUMBER_TOUCH_KEY ) * (i + 1)) - 15;
		sitronix_key_array[i].y_low = ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y;
		sitronix_key_array[i].y_high = ts->resolution_y;
		DbgMsg("key[%d] %d, %d, %d, %d\n", i, sitronix_key_array[i].x_low, sitronix_key_array[i].x_high, sitronix_key_array[i].y_low, sitronix_key_array[i].y_high);
		set_bit(sitronix_key_array[i].code, ts->keyevent_input->keybit);

	}
	max_x = ts->resolution_x;
	max_y = ts->resolution_y - ts->resolution_y / SCALE_KEY_HIGH_Y;
#endif // SITRONIX_KEY_BOUNDARY_MANUAL_SPECIFY
#endif // SITRONIX_TOUCH_KEY
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	ret = input_register_device(ts->keyevent_input);
	if(ret < 0){
		printk("Can not register key input device.");
		goto err_input_register_device_failed;
	}
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)

	__set_bit(ABS_MT_TOUCH_MAJOR, ts->input_dev->absbit);
	__set_bit(ABS_MT_WIDTH_MAJOR, ts->input_dev->absbit);
	__set_bit(ABS_MT_POSITION_X, ts->input_dev->absbit);
	__set_bit(ABS_MT_POSITION_Y, ts->input_dev->absbit);
	__set_bit(ABS_MT_TOOL_TYPE, ts->input_dev->absbit);
	__set_bit(ABS_MT_BLOB_ID, ts->input_dev->absbit);
	__set_bit(ABS_MT_TRACKING_ID, ts->input_dev->absbit);

	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0,  255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, ts->max_touches, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);

	ret = input_register_device(ts->input_dev);
	if(ret < 0){
		printk("Can not register input device.");
		goto err_input_register_device_failed;
	}

	ts->suspend_state = 0;
	if (client->irq){
#ifdef SITRONIX_LEVEL_TRIGGERED
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_LOW | IRQF_DISABLED, client->name, ts);
#else
		ret = request_irq(client->irq, sitronix_ts_irq_handler, IRQF_TRIGGER_FALLING | IRQF_DISABLED, client->name, ts);
#endif // SITRONIX_LEVEL_TRIGGERED
		if (ret == 0){
			sitronix_ts_irq_on = 1;
			ts->use_irq = 1;
		}else
			dev_err(&client->dev, "request_irq failed\n");
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
        ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
        ts->early_suspend.suspend = sitronix_ts_early_suspend;
        ts->early_suspend.resume = sitronix_ts_late_resume;
        register_early_suspend(&ts->early_suspend);
#endif // CONFIG_HAS_EARLYSUSPEND
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	input_free_device(ts->keyevent_input);
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
err_input_dev_alloc_failed:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
#ifdef SITRONIX_MONITOR_THREAD
	if(SitronixMonitorThread){
	      kthread_stop(SitronixMonitorThread);
	      SitronixMonitorThread = NULL;
      	}
#endif // SITRONIX_MONITOR_THREAD
err_device_info_error:
	return ret;
}

static int sitronix_ts_remove(struct i2c_client *client)
{
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif // CONFIG_HAS_EARLYSUSPEND
#ifdef SITRONIX_MONITOR_THREAD
	if(SitronixMonitorThread){
	      kthread_stop(SitronixMonitorThread);
	      SitronixMonitorThread = NULL;
      	}
#endif // SITRONIX_MONITOR_THREAD
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
#if defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	input_unregister_device(ts->keyevent_input);
#endif // defined(SITRONIX_SENSOR_KEY) || defined (SITRONIX_TOUCH_KEY)
	kfree(ts);
	return 0;
}

static int sitronix_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);

	DbgMsg("%s\n", __FUNCTION__);
#ifdef SITRONIX_MONITOR_THREAD
	if(SitronixMonitorThread){
		kthread_stop(SitronixMonitorThread);
		SitronixMonitorThread = NULL;
	}
	sitronix_ts_delay_monitor_thread_start = DELAY_MONITOR_THREAD_START_RESUME;
#endif // SITRONIX_MONITOR_THREAD
	if(ts->use_irq){
		sitronix_ts_irq_on = 0;
		disable_irq_nosync(ts->client->irq);
	}
	ts->suspend_state = 1;

	ret = sitronix_ts_set_powerdown_bit(ts, 1);
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	gpio_direction_output(irq_to_gpio(client->irq), 1);
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT
	DbgMsg("%s return\n", __FUNCTION__);

	return 0;
}

static int sitronix_ts_resume(struct i2c_client *client)
{
#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	unsigned int gpio;
#else
	int ret;
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT
	struct sitronix_ts_data *ts = i2c_get_clientdata(client);

	DbgMsg("%s\n", __FUNCTION__);

#ifdef SITRONIX_WAKE_UP_TOUCH_BY_INT
	gpio = irq_to_gpio(client->irq);
	gpio_set_value(gpio, 0);
	gpio_direction_input(gpio);
#else
	ret = sitronix_ts_set_powerdown_bit(ts, 0);
#endif // SITRONIX_WAKE_UP_TOUCH_BY_INT

	ts->suspend_state = 0;
	if(ts->use_irq){
		sitronix_ts_irq_on = 1;
		enable_irq(ts->client->irq);
	}
#ifdef SITRONIX_MONITOR_THREAD
	atomic_set(&iMonitorThreadPostpone,1);
	SitronixMonitorThread = kthread_run(sitronix_ts_monitor_thread,"Sitronix","Monitorthread");
	if(IS_ERR(SitronixMonitorThread))
		SitronixMonitorThread = NULL;
#endif // SITRONIX_MONITOR_THREAD
	DbgMsg("%s return\n", __FUNCTION__);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sitronix_ts_early_suspend(struct early_suspend *h)
{
	struct sitronix_ts_data *ts;
	DbgMsg("%s\n", __FUNCTION__);
	ts = container_of(h, struct sitronix_ts_data, early_suspend);
	sitronix_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void sitronix_ts_late_resume(struct early_suspend *h)
{
	struct sitronix_ts_data *ts;
	DbgMsg("%s\n", __FUNCTION__);
	ts = container_of(h, struct sitronix_ts_data, early_suspend);
	sitronix_ts_resume(ts->client);
}
#endif // CONFIG_HAS_EARLYSUSPEND

static const struct i2c_device_id sitronix_ts_id[] = {
	{ SITRONIX_I2C_TOUCH_DRV_NAME, 0 },
	{ }
};

static struct i2c_driver sitronix_ts_driver = {
	.probe		= sitronix_ts_probe,
	.remove		= sitronix_ts_remove,
	.id_table	= sitronix_ts_id,
	.driver = {
		.name	= SITRONIX_I2C_TOUCH_DRV_NAME,
	},
};

#ifdef SITRONIX_FW_UPGRADE_FEATURE
static struct file_operations nc_fops = {
	.owner =        THIS_MODULE,
	.write		= sitronix_write,
	.read		= sitronix_read,
	.open		= sitronix_open,
	.unlocked_ioctl = sitronix_ioctl,
	.release	= sitronix_release,
};
#endif // SITRONIX_FW_UPGRADE_FEATURE

static int __devinit sitronix_ts_init(void)
{
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	int result;
	int err = 0;
#endif // SITRONIX_FW_UPGRADE_FEATURE
	printk("Sitronix touch driver %d.%d.%d\n", DRIVER_MAJOR, DRIVER_MINOR, DRIVER_PATCHLEVEL);
	printk("Release date: %s\n", DRIVER_DATE);
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	dev_t devno = MKDEV(sitronix_major, 0);
	result  = alloc_chrdev_region(&devno, 0, 1, SITRONIX_I2C_TOUCH_DEV_NAME);
	if(result < 0){
		printk("fail to allocate chrdev (%d) \n", result);
		return 0;
	}
	sitronix_major = MAJOR(devno);
        cdev_init(&sitronix_cdev, &nc_fops);
	sitronix_cdev.owner = THIS_MODULE;
	sitronix_cdev.ops = &nc_fops;
        err =  cdev_add(&sitronix_cdev, devno, 1);
	if(err){
		printk("fail to add cdev (%d) \n", err);
		return 0;
	}

	sitronix_class = class_create(THIS_MODULE, SITRONIX_I2C_TOUCH_DEV_NAME);
	if (IS_ERR(sitronix_class)) {
		result = PTR_ERR(sitronix_class);
		unregister_chrdev(sitronix_major, SITRONIX_I2C_TOUCH_DEV_NAME);
		printk("fail to create class (%d) \n", result);
		return result;
	}
	device_create(sitronix_class, NULL, MKDEV(sitronix_major, 0), NULL, SITRONIX_I2C_TOUCH_DEV_NAME);
#endif // SITRONIX_FW_UPGRADE_FEATURE
	return i2c_add_driver(&sitronix_ts_driver);
}

static void __exit sitronix_ts_exit(void)
{
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	dev_t dev_id = MKDEV(sitronix_major, 0);
#endif // SITRONIX_FW_UPGRADE_FEATURE
	i2c_del_driver(&sitronix_ts_driver);
#ifdef SITRONIX_FW_UPGRADE_FEATURE
	cdev_del(&sitronix_cdev);

	device_destroy(sitronix_class, dev_id); //delete device node under /dev
	class_destroy(sitronix_class); //delete class created by us
	unregister_chrdev_region(dev_id, 1);
#endif // SITRONIX_FW_UPGRADE_FEATURE
}

module_init(sitronix_ts_init);
module_exit(sitronix_ts_exit);

MODULE_DESCRIPTION("Sitronix Multi-Touch Driver");
MODULE_LICENSE("GPL");
