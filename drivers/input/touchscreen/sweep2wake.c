/*
 * drivers/input/touchscreen/sweep2wake.c
 *
 *
 * Copyright (c) 2012, Dennis Rassmann <showp1984@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * History:
 *	Added sysfs adjustments for different sweep styles
 * 		by paul reioux (aka Faux123) <reioux@gmail.com>
 * 
 *      Added gesture_s2w. So, now you must not change a keyboard drivers.
 * 		by RomanGavdulskiy. You can write me to <romangavdulskiy@gmail.com> or follow me on the GitHub <RomanGavdulskiy>. 
 * */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/input/sweep2wake.h>

/* Tuneables */
#define DEBUG                   1
#define DEFAULT_S2W_Y_LIMIT             800
#define DEFAULT_S2W_X_MAX               450
#define DEFAULT_S2W_X_B1                50
#define DEFAULT_S2W_X_B2                150
#define DEFAULT_S2W_X_FINAL             50
#define DEFAULT_S2W_PWRKEY_DUR          60

/* Resources */
int sweep2wake = 0;
int doubletap2wake = 0;
int dt2w_changed = 0;
bool scr_suspended = false, exec_count = true;
bool scr_on_touch = false, barrier[2] = {false, false};
static struct input_dev * sweep2wake_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);

unsigned long initial_time = 0;
unsigned long dt2w_time[2] = {0, 0};
unsigned int dt2w_x[2] = {0, 0};
unsigned int dt2w_y[2] = {0, 0};
#define S2W_TIMEOUT 75
#define DT2W_TIMEOUT_MAX 120
#define DT2W_TIMEOUT_MIN 12
#define DT2W_DELTA 75
#define S2W_PWRKEY_DUR 100

//#ifdef CONFIG_CMDLINE_OPTIONS
/* Read cmdline for s2w */
static int __init read_s2w_cmdline(char *s2w)
{
	if (strcmp(s2w, "2") == 0) {
		printk(KERN_INFO "[cmdline_s2w]: Sweep2Wake/Sweep2Sleep enabled. | s2w='%s'", s2w);
		sweep2wake = 2;
	} else if (strcmp(s2w, "1") == 0) {
		printk(KERN_INFO "[cmdline_s2w]: Sweep2Wake enabled. | s2w='%s'", s2w);
		sweep2wake = 1;
	} else if (strcmp(s2w, "0") == 0) {
		printk(KERN_INFO "[cmdline_s2w]: Sweep2Wake disabled. | s2w='%s'", s2w);
		sweep2wake = 0;
	}else {
		printk(KERN_INFO "[cmdline_s2w]: No valid input found. Sweep2Wake disabled. | s2w='%s'", s2w);
		sweep2wake = 0;
	}
	return 1;
}
__setup("s2w=", read_s2w_cmdline);
//#endif

/* PowerKey setter */
void sweep2wake_setdev(struct input_dev * input_device) {
 sweep2wake_pwrdev = input_device;
 return;
}
EXPORT_SYMBOL(sweep2wake_setdev);

/* PowerKey work func */
static void sweep2wake_presspwr(struct work_struct * sweep2wake_presspwr_work) {
 if (!mutex_trylock(&pwrkeyworklock))
 return;
 input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 1);
 input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
 msleep(S2W_PWRKEY_DUR);
 input_event(sweep2wake_pwrdev, EV_KEY, KEY_POWER, 0);
 input_event(sweep2wake_pwrdev, EV_SYN, 0, 0);
 msleep(S2W_PWRKEY_DUR);
 mutex_unlock(&pwrkeyworklock);
 return;
}
static DECLARE_WORK(sweep2wake_presspwr_work, sweep2wake_presspwr);

/* PowerKey trigger */
void sweep2wake_pwrtrigger(void) {
 schedule_work(&sweep2wake_presspwr_work);
 return;
}
static void reset_sweep2wake(void)
{
printk("[SWEEP2WAKE]: ressetting in D2W\n");
    pr_info("[sweep2wake]: line : %d | func : %s\n", __LINE__, __func__);

	//reset doubletap2wake
	dt2w_time[0] = 0;
	dt2w_x[0] = 0;
	dt2w_y[0] = 0;
	dt2w_time[1] = 0;
	dt2w_x[1] = 0;
	dt2w_y[1] = 0;
	initial_time = 0;
    return;
}

	/* Sweep2wake main function */
void detect_sweep2wake(int x, int y)
{
 int prevx = 0, nextx = 0;
 			  
#if DEBUG
 pr_info("[sweep2wake]: x,y(%4d,%4d)\n",
 x, y);
#endif
 //left->right
 if ((scr_suspended == true) && (sweep2wake > 0)) {
		  prevx = 0;
		  nextx = DEFAULT_S2W_X_B1;
 if ((barrier[0] == true) || ((x > prevx) && (x < nextx) && (y > 0))) {
		  prevx = nextx;
		  nextx = DEFAULT_S2W_X_B2;
		  barrier[0] = true;
 if ((barrier[1] == true) || ((x > prevx) && (x < nextx) && (y > 0))) {
		  prevx = nextx;
		  barrier[1] = true;
 if ((x > prevx) && (y > 0)) {
 if (x > (DEFAULT_S2W_X_MAX - DEFAULT_S2W_X_FINAL)) {
 if (exec_count) {
		  printk(KERN_INFO "[sweep2wake]: ON");
		  sweep2wake_pwrtrigger();
		  exec_count = false;
	}
      }
    }
  }
 }
 //right->left
 } else if ( (scr_suspended == false) && (sweep2wake > 0)) {
		scr_on_touch=true;
		prevx = (DEFAULT_S2W_X_MAX - DEFAULT_S2W_X_FINAL);
		nextx = DEFAULT_S2W_X_B2;
 if ((barrier[0] == true) || ((x < prevx) &&(x > nextx) && (y > DEFAULT_S2W_Y_LIMIT))) {
		prevx = nextx;
		nextx = DEFAULT_S2W_X_B1;
		barrier[0] = true;
 if ((barrier[1] == true) ||((x < prevx) && (x > nextx) && (y > DEFAULT_S2W_Y_LIMIT))) {
		prevx = nextx;
		barrier[1] = true;
 if ((x < prevx) && (y > DEFAULT_S2W_Y_LIMIT)) {
 if (x < DEFAULT_S2W_X_FINAL) {
 if (exec_count) {
	      printk(KERN_INFO "[sweep2wake]: OFF");
	      sweep2wake_pwrtrigger();
	      exec_count = false;
	  }
	}
      }
    }
  }
 }
}
void doubletap2wake_func(int x, int y, unsigned long time)
{

	int delta_x = 0;
	int delta_y = 0;

	printk("[dt2wake]: x,y(%d,%d) jiffies:%lu\n", x, y, time);

        dt2w_time[1] = dt2w_time[0];
        dt2w_time[0] = time;

	if (!initial_time)
		initial_time = time;	

	if (time - initial_time > 800)
		reset_sweep2wake();
printk("[SWEEP2WAKE]: d2w reset\n");
	
	if ((dt2w_time[0] - dt2w_time[1]) < 10)
		return;
printk("[SWEEP2WAKE]: checking d2w\n");
	dt2w_x[1] = dt2w_x[0];
    dt2w_x[0] = x;
	dt2w_y[1] = dt2w_y[0];
    dt2w_y[0] = y;

	delta_x = (dt2w_x[0]-dt2w_x[1]);
	delta_y = (dt2w_y[0]-dt2w_y[1]);

        if (scr_suspended && doubletap2wake > 0) {
printk("[SWEEP2WAKE]: y = %d, timedelta = %lu, deltax= %d, deltay = %d\n",y,(dt2w_time[0] - initial_time),delta_x,delta_y);
		if (y > 50 && y < 1200
			&& ((dt2w_time[0] - initial_time) > DT2W_TIMEOUT_MIN)
			&& ((dt2w_time[0] - initial_time) < DT2W_TIMEOUT_MAX)
			&& (abs(delta_x) < DT2W_DELTA)
			&& (abs(delta_y) < DT2W_DELTA)
			) {
                printk("[DT2W]: OFF->ON\n");
                 sweep2wake_pwrtrigger();
		}
	}
        return;
}


/********************* SYSFS INTERFACE ***********************/
static ssize_t sweep2wake_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%i\n", sweep2wake);
}



static ssize_t sweep2wake_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;
	if(sscanf(buf, "%i\n", &data) == 1) 	  
	sweep2wake = data; 
	
	else
		pr_info("%s: unknown input!\n", __FUNCTION__);
	return count;
}

static ssize_t doubletap2wake_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%i\n", doubletap2wake);
}

static ssize_t doubletap2wake_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int data;
	if(sscanf(buf, "%i\n", &data) == 1)
	  doubletap2wake = data;
	  else
		pr_info("%s: unknown input!\n", __FUNCTION__);
	return count;
}

static struct kobj_attribute sweep2wake_attribute =
	__ATTR(sweep2wake,
		0666,
		sweep2wake_show,
		sweep2wake_store);

static struct kobj_attribute doubletap2wake_attribute =
	__ATTR(doubletap2wake,
		0666,
		doubletap2wake_show,
		doubletap2wake_store);

static struct attribute *s2w_parameters_attrs[] =
	{
		
		&sweep2wake_attribute.attr,
		&doubletap2wake_attribute.attr,

		NULL,
	};

static struct attribute_group s2w_parameters_attr_group =
	{
		.attrs = s2w_parameters_attrs,
	};

static struct kobject *s2w_parameters_kobj;

/*
 * INIT / EXIT stuff below here
 */

static int __init sweep2wake_init(void)
{
	int sysfs_result;
	int err;
	/*start add gesture_s2w */
        struct input_dev *gesture_s2w_input_dev;
	/*end add gesture_s2w */
	s2w_parameters_kobj = kobject_create_and_add("android_touch", NULL);
	if (!s2w_parameters_kobj) {
		pr_err("%s kobject create failed!\n", __FUNCTION__);
		return -ENOMEM;
        }

	sysfs_result = sysfs_create_group(s2w_parameters_kobj, &s2w_parameters_attr_group);

        if (sysfs_result) {
		pr_info("%s sysfs create failed!\n", __FUNCTION__);
		kobject_put(s2w_parameters_kobj);
	}

	pr_info("[sweep2wake]: %s done\n", __func__);
	/*start add gesture_s2w */
	gesture_s2w_input_dev = input_allocate_device();
	if (!gesture_s2w_input_dev) {
		printk("==input_allocate_device for gesture key failed=\n");
	}
	set_bit(KEY_POWER, gesture_s2w_input_dev->keybit);
	set_bit(EV_KEY, gesture_s2w_input_dev->evbit);
	gesture_s2w_input_dev->name		= "gesture_s2w";
	err = input_register_device(gesture_s2w_input_dev);
	if (err) {
		printk("==input_register_device gesture_s2w key failed=\n");
	}
	sweep2wake_setdev(gesture_s2w_input_dev);
	printk(KERN_INFO "[sweep2wake]: set device %s\n", gesture_s2w_input_dev->name);
	/*end add gesture_s2w */
	return sysfs_result;
}

static void __exit sweep2wake_exit(void)
{
	return;
}

module_init(sweep2wake_init);
module_exit(sweep2wake_exit);

MODULE_DESCRIPTION("Sweep2wake");
MODULE_LICENSE("GPLv2");