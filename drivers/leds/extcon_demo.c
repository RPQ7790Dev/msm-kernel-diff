#include <linux/module.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/async.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/input/mt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/extcon-provider.h>

/*驱动注册的头文件，包含驱动的结构体和注册和卸载的函数*/
#include <linux/platform_device.h>

#define DRIVER_NAME "extcon_demo"

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("TOPEET");


struct extcon_dev *edev;
struct extcon_dev *edev1;
int my_status = 0;

static struct class *my_class;
static struct device *my_device;

struct notifier_block extcon1_nb;

static const unsigned int headset_cable[] = {
	EXTCON_JACK_MICROPHONE,
	EXTCON_JACK_HEADPHONE,
	EXTCON_NONE,
};

static const unsigned int extcon_demo_cable[] = {
	EXTCON_DISP_DP,
	EXTCON_DEMO_A,
	EXTCON_NONE,
};

static const unsigned int extcon_demo_cable1[] = {
	EXTCON_DEMO_A,
	EXTCON_NONE,
};

static int extcon1_notify(struct notifier_block *nb,
						  unsigned long event, void *data)
{
	int state = extcon_get_state(edev1, EXTCON_DEMO_A);  //获取dp物理上连接状态
	if(!!state){ //dp物理上插入
		if(!my_status){  //my_status为0表示走dp audio
			printk(KERN_ERR "jaden extcon1_notify dp attach");
			extcon_set_state_sync(edev,EXTCON_DISP_DP, true);
		}
	}else{ //dp物理上拔出
		if(!my_status){ //my_status为0表示走dp audio
			printk(KERN_ERR "jaden extcon1_notify dp detach");
			extcon_set_state_sync(edev,EXTCON_DISP_DP, false);
		}
	}
	// 处理 extcon1 事件
	return NOTIFY_OK;
}

static ssize_t extcon_demo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "extcon_demo_show!\n");
	//extcon_set_state_sync(edev,
	//				      EXTCON_JACK_MICROPHONE, true);
	return snprintf(buf, PAGE_SIZE, "%d\n", my_status);
	return 0;
}

static ssize_t extcon_demo_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int tmp;
	int state;
	int state1;

	if (sscanf(buf, "%d", &tmp) != 1) {
		return -EINVAL; // 如果解析失败，返回错误
	}

	my_status = !!tmp; //my_status为0表示走dpaudio,为1表示走speaker

	//extcon_set_state_sync(edev,
	//EXTCON_JACK_MICROPHONE, false);
	state = extcon_get_state(edev1, EXTCON_DEMO_A); //获取dp物理上连接状态
	state1 = extcon_get_state(edev, EXTCON_DISP_DP); //获取当前audio状态，1为dp,0为speaker

	printk(KERN_ERR "jaden Successfully got dp_conn state:%d,audio_cur_state:%d,my_status:%d,tmp:%d\n",state,state1,my_status,tmp);

	if(state){ //dp连接上才生效
		if(my_status && state1){ //走speaker且目前audio是dp才切换到speaker,目前是speaker不用切换
			printk(KERN_ERR "jaden extcon_demo_store! switch to speaker\n");
			extcon_set_state_sync(edev,EXTCON_DISP_DP, false);
		}
		if(!my_status && !state1){ //走dp且目前audio是speaker才切换到dp,目前是dp不用切换
			printk(KERN_ERR "jaden extcon_demo_store! switch to dp audio\n");
			extcon_set_state_sync(edev,EXTCON_DISP_DP, true);
		}
	}


	return count;
}
static DEVICE_ATTR(state, 0664, extcon_demo_show, extcon_demo_store);

void quectel_dp_attach(void){
	extcon_set_state_sync(edev,EXTCON_DISP_DP, true);
}

void quectel_dp_detach(void){
	extcon_set_state_sync(edev,EXTCON_DISP_DP, false);
}

static int extcon_demo_probe(struct platform_device *pdv){
	int err;
	int ret;
	printk(KERN_ERR "\tinitialized\n");


	//edev = devm_extcon_dev_allocate(&pdv->dev, headset_cable);//extcon_demo_cable
	edev = devm_extcon_dev_allocate(&pdv->dev, extcon_demo_cable);//

	ret = devm_extcon_dev_register(&pdv->dev, edev);

	err = device_create_file(&pdv->dev, &dev_attr_state);
	if (err)
		printk(KERN_ERR "\tdevice_create_file_error\n");
	else
		printk(KERN_ERR "\t very good device_create_file_success\n");

	my_class = class_create("my_class");
	if (IS_ERR(my_class)) {
		printk(KERN_ALERT "jaden Failed to create class\n");
		return PTR_ERR(my_class);
	}
	my_device = device_create(my_class, NULL, 0, NULL, "cus_dp_conn");
	if (IS_ERR(my_device)) {
		class_destroy(my_class);
		printk(KERN_ALERT "jaden Failed to create device\n");
		return PTR_ERR(my_device);
	}
	edev1 = devm_extcon_dev_allocate(my_device, extcon_demo_cable1);//

	ret = devm_extcon_dev_register(my_device, edev1);

	extcon1_nb.notifier_call = extcon1_notify;
	ret = extcon_register_notifier(edev1, EXTCON_DEMO_A, &extcon1_nb);
	//if (ret)
	//return dev_err_probe(&pdv->dev, ret, "jaden Failed to register extcon1 notifier\n");
	return 0;
}

static int extcon_demo_remove(struct platform_device *pdv){

	return 0;
}
static void extcon_demo_shutdown(struct platform_device *pdv){

	;
}

static int extcon_demo_resume(struct platform_device *pdv){

	return 0;
}

static const struct of_device_id extcon_demo_of_match[] = {
	{ .compatible = "extcon_demo", },
	{},
};

MODULE_DEVICE_TABLE(of, extcon_demo_of_match);

struct platform_driver extcon_demo_driver = {
	.probe = extcon_demo_probe,
	.remove = extcon_demo_remove,
	.shutdown = extcon_demo_shutdown,
	.resume = extcon_demo_resume,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(extcon_demo_of_match),
	}
};


static int extcon_demo_init(void)
{
	int DriverState;

	printk(KERN_ERR "HELLO WORLD enter! extcon_demo\n");
	DriverState = platform_driver_register(&extcon_demo_driver);

	printk(KERN_ERR "\t DriverState  extcon_demo is %d\n",DriverState);
	return 0;
}

static void extcon_demo_exit(void)
{
	printk(KERN_ERR "HELLO WORLD exit!\n");

	platform_driver_unregister(&extcon_demo_driver);
}

module_init(extcon_demo_init);
module_exit(extcon_demo_exit);
