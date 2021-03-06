/*
 * Report_Coordinate
 */
#include "include/CN1100_common.h"
#include "include/CN1100_linux.h"
#include "include/CN1100_Function.h"
int SCREEN_HIGH = 1024;
int SCREEN_WIDTH = 600;
int CN1100_RESET_PIN = 225;//PH
int CN1100_INT_PIN = 37;//PB05

static struct i2c_board_info tc1126_board_info[] __initdata = {
	{
		I2C_BOARD_INFO("tc1126", 0x20),
	}
};


static int screen_max_x = 1024;
static int screen_max_y = 600;
static int revert_x_flag = 1;
//static int revert_y_flag = 1;
static int twi_id = 0;
//static int exchange_x_y_flag = 1;

//#define CTP_HAVE_TOUCH_KEY
#ifdef CTP_HAVE_TOUCH_KEY

static int key_pressed[5] = {0};
//static int touch_keys[] = {
//    KEY_BACK,KEY_HOME,KEY_MENU,KEY_SEARCH,
//};

struct keys{
    uint16_t key;
    uint16_t xmin;
    uint16_t xmax;
    uint16_t ymin;
    uint16_t ymax;
};

static struct keys chm_ts_keys[]={
    {KEY_BACK,0,340,500,600},
    {KEY_HOMEPAGE,340,680,500,600},
    {KEY_MENU,680,1024,500,600},
};

#define MAX_KEY_NUM ((sizeof(chm_ts_keys)/sizeof(chm_ts_keys[0])))

static int touch_key_pressed = 0;

#endif

static const unsigned short normal_i2c[2] = {0x20,I2C_CLIENT_END};

//#define REPORT_DATA_ANDROID_4_0
struct cn1100_spi_dev *spidev = NULL;
uint16_t chip_addr = 0x20;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void chm_ts_early_suspend(struct early_suspend *h);
static void chm_ts_late_resume(struct early_suspend *h);
#endif

void update_cfg(void){
    int i = 0;
    cn1100_t config[] = { 
#include "chm_ts.cfg" 
    };  
    for(i = 0;i < ARRAY_SIZE(config);i++){
        chm_ts_write_config(config[i]);
        msleep(10);
    }   
    bdt.BFD.InitCount = 0;
}
int ctp_wakeup(int status)
{
    if (status == 1) { 
        __gpio_set_value(225, 1);
    }else{
        __gpio_set_value(225,0);
    }
    msleep(10);

    return 0;
}

int SPI_read_singleData(uint32_t addr)
{
    /*Test to see if sth wrong,change FAST_READ_DATA to READ_CHIP_ID*/
    int16_t ret = 0;
    uint8_t tmp[] = {
        ((addr>>8)&0xff),addr&0xff,
    };
    struct i2c_msg *msgs;
    int status = 0;
    uint8_t rx[8] = {0};
    msgs = kmalloc(sizeof(struct i2c_msg)*I2C_MSGS,GFP_KERNEL);
    msgs[0].addr = chip_addr;
    msgs[0].flags = 0;
    msgs[0].buf = tmp;
    msgs[0].len = ARRAY_SIZE(tmp);

    msgs[1].addr = chip_addr;
    msgs[1].flags =  I2C_M_RD;
    msgs[1].buf = rx;
    msgs[1].len = 2; 

    if(!spidev->client){
        CN1100_print("cannot get i2c adapter\n");
        ret = -1;
        goto out;
    }
    ret = 0;
    while(ret < 3){
        status = i2c_transfer(spidev->client->adapter,msgs,2);
        if(status !=  2){
            ret++;
            CN1100_print("retry:%d\n",ret);
        }else{
            break;
        }
    }
    if(ret >= 3){
        CN1100_print("s-failed to send i2c message:%d\n",status);
        ret = status;
        goto out;
    }
    ret = rx[0] << 8;
    ret +=rx[1];
out:
    kfree(msgs);
    return ret;
}

int SPI_read_DATAs(uint32_t addr, uint16_t num, uint16_t *data)
{
    uint32_t i = 0,j = 0;

    uint8_t rx[512] = {0};
    uint8_t tmp[] = {
        (addr>>8)&0xff,(addr)&0xff,
    };
    struct i2c_msg *msgs;
    int status = 0,ret = 0;

    msgs = kmalloc(sizeof(struct i2c_msg)*I2C_MSGS,GFP_KERNEL);

    msgs[0].addr = chip_addr;
    msgs[0].flags = 0;
    msgs[0].buf = tmp;
    msgs[0].len = ARRAY_SIZE(tmp);

    msgs[1].addr = chip_addr;
    msgs[1].flags =  I2C_M_RD;
    msgs[1].buf = rx;
    msgs[1].len = 2*num;

    if(!spidev->client->adapter){
        CN1100_print("cannot get i2c adapter\n");
        status = -1;
        goto out;
    }
    ret = 0;
    while(ret < 3){
        status = i2c_transfer(spidev->client->adapter,msgs,2);
        if(status !=  2){
            ret++;
            CN1100_print("retry:%d\n",ret);
        }else{
            break;
        }
    }
    if(ret >= 3){
        CN1100_print("m-failed to send i2c message:%d\n",status);
        ret = status;
        goto out;
    }
    j = 0;
    for(i=0;i<num;i++){
        data[i] = (uint16_t)rx[j++]<<8;
        data[i] += (uint16_t)rx[j++];
    }
out:
    kfree(msgs);
    return status;
}

int SPI_write_singleData(uint32_t addr, uint16_t data)
{
    uint8_t tmp[] = {
        ((addr>>8)&0xff),addr&0xff,((data>>8)&0xff),data&0xff,
    };
    struct i2c_msg msg;
    int status = 0,ret = 0;
    msg.addr = chip_addr;
    msg.flags = 0;
    msg.buf = tmp;
    msg.len = ARRAY_SIZE(tmp);

    if(!spidev->client->adapter){
        CN1100_print("cannot get i2c adapter\n");
        status = -1;
        goto out;
    }
    ret = 0;
    while(ret < 3){
        status = i2c_transfer(spidev->client->adapter,&msg,1);
        if(status !=  1){
            ret++;
            CN1100_print("retry:%d\n",ret);
        }else{
            break;
        }
    }
    if(ret >= 3){
        CN1100_print("failed to send i2c message:%d\n",status);
    }
out:
    return status;
}

int SPI_write_DATAs(uint32_t addr, uint16_t num, uint16_t *data)
{
    uint32_t i =0 ,count = 0,j = 0;
    int status = 0,ret = 0;
    uint8_t tmp[] = {
        ((addr>>8)&0xff),(addr&0xff),
    };
    struct i2c_msg msg;
    uint8_t tx[512] = {0};
    for(i=0;i<ARRAY_SIZE(tmp);i++){
        tx[i] = tmp[i];
    }

    for(j=0;j<num;j++){
        tx[i++] = ((data[j]>>8)&0xff);
        tx[i++] = (data[j])&0xff;
    }
    count = i;
    msg.addr = chip_addr;
    msg.flags = 0;
    msg.buf = tx;
    msg.len = count;

    if(!spidev->client->adapter){
        CN1100_print("cannot get i2c adapter\n");
        status = -1;
        goto out;
    }
    ret = 0;
    while(ret < 3){
        status = i2c_transfer(spidev->client->adapter,&msg,1);
        if(status !=  1){
            ret++;
            CN1100_print("retry:%d\n",ret);
        }else{
            break;
        }
    }
    if(ret >= 3){
        CN1100_print("failed to send i2c message:%d\n",status);
    }
out:
    return status;
}

#ifdef CTP_HAVE_TOUCH_KEY
void report_key(int X,int Y,int id)
{
    int i = 0;
    for(i = 0;i < MAX_KEY_NUM;i++){
        if(X>chm_ts_keys[i].xmin&&X<chm_ts_keys[i].xmax){
            input_report_key(spidev->dev,chm_ts_keys[i].key,1);
            key_pressed[id] = chm_ts_keys[i].key;
            touch_key_pressed = 1;
            CN1100_print("KEY_PRESSED:%d\n",chm_ts_keys[i].key);
            break;
        }
    }
}
#endif
static int irqcalls=0;
void Report_Coordinate_Wait4_SingleTime(int id,int X, int Y)
{
    Y  = (uint16_t)(( ((uint32_t)Y) * RECV_SCALE )>>16);
    X  = (uint16_t)(( ((uint32_t)X) * XMTR_SCALE )>>16);
    
    if(X > 0 || Y > 0){ 
        X  = revert_x_flag ?  screen_max_x - X  : X;
        input_report_abs(spidev->dev, ABS_MT_TRACKING_ID,id);
        //input_report_abs(spidev->dev, ABS_MT_TOUCH_MAJOR, 5);
        input_mt_slot(spidev->dev, id);
        input_mt_report_slot_state(spidev->dev, MT_TOOL_FINGER, true); 
        input_report_abs(spidev->dev, ABS_MT_POSITION_X, X);
        input_report_abs(spidev->dev, ABS_MT_POSITION_Y, Y); 
        input_report_abs(spidev->dev, ABS_X, X);
        input_report_abs(spidev->dev, ABS_Y, Y); 
        //input_report_abs(spidev->dev, ABS_MT_WIDTH_MAJOR, 5); 
        CN1100_print("IrqCalls %d\n",irqcalls);

    }else{
    //input_mt_sync(spidev->dev);  TODO ???
    }   
}
uint16_t FingProc_Dist2PMeasure(int16_t x1, int16_t y1, int16_t x2, int16_t y2);


void Report_Coordinate()
{
    int fnum = FINGER_NUM_MAX;
    int X=0, Y=0, i;



    //*************************************
    // Report Old 4 Point with others
    //*************************************
    for(i=0; i<fnum; i++) 
    {    
        Y  = bdt.DPD[i].Finger_Y_Reported; // Y -> RECV (480)
        X  = bdt.DPD[i].Finger_X_Reported; // X -> XTMR (800) 
        //Report_Coordinate_Wait4_SingleTime(i,X, Y);
        Y  = (uint16_t)(( ((uint32_t)Y) * RECV_SCALE )>>16);
        X  = (uint16_t)(( ((uint32_t)X) * XMTR_SCALE )>>16);
        if(X > 0 || Y > 0){ 
            X  = revert_x_flag ?  screen_max_x - X  : X;
            //input_report_abs(spidev->dev, ABS_MT_TRACKING_ID,i);
            //input_report_abs(spidev->dev, ABS_MT_TOUCH_MAJOR, 5);
            input_mt_slot(spidev->dev, i);
            input_mt_report_slot_state(spidev->dev, MT_TOOL_FINGER, true); 
            input_report_abs(spidev->dev, ABS_MT_POSITION_X, X);
            input_report_abs(spidev->dev, ABS_MT_POSITION_Y, Y); 
            input_report_abs(spidev->dev, ABS_X, X);
            input_report_abs(spidev->dev, ABS_Y, Y); 
            //input_report_abs(spidev->dev, ABS_MT_WIDTH_MAJOR, 5); 
            CN1100_print("IrqCalls %d\n",irqcalls);

            }else{
                //input_mt_sync(spidev->dev);  TODO ???
            }   

    }
    input_mt_sync_frame(spidev->dev);
    input_sync(spidev->dev);
}


static irqreturn_t cn1100_irq_handler(int irq, void *dev_id)
{
    if(spidev->i2c_ok){
        disable_irq_nosync(spidev->irq);
#ifdef CAL_TIME_CONSUMED
        if(0 == spidev->irq_interval){
            do_gettimeofday(&spidev->pretime);
            spidev->irq_interval = 1;
        }else{
            do_gettimeofday(&spidev->curtime);
            spidev->irq_interval = (spidev->curtime.tv_sec-spidev->pretime.tv_sec)*1000000+(spidev->curtime.tv_usec-spidev->pretime.tv_usec);
            spidev->pretime = spidev->curtime;
        }   
#endif
        queue_work(spidev->workqueue,&spidev->main);
    }
    irqcalls++;   
    return IRQ_RETVAL(IRQ_HANDLED);
}
/*#ifdef DEBUG_PROC_USED
static int chm_proc_open(struct inode *inode, struct file *file)
{
    return 0;
}

int chm_proc_release(struct inode *inode, struct file *file)
{
    return 0;
}

static const struct file_operations chm_proc_fops = {
    .owner      = THIS_MODULE,
    .open       = chm_proc_open,
    .read       = chm_proc_read,
    .write      = chm_proc_write,
    .release    = chm_proc_release,
};
#endif*/


static int chm_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int status = 0;
    int value = 0;
//    int index = 0;
    spidev->mode = CN1100_USE_IRQ;
    spidev->client = client;
    if (!i2c_check_functionality(spidev->client->adapter, I2C_FUNC_I2C)) {
        dev_err(&spidev->client->dev, "I2C functionality not supported\n");
        return -ENODEV;
    }    
	ctp_wakeup(0);
	msleep(10);
	ctp_wakeup(1);
    chip_addr = 0x20;
    value = SPI_read_singleData(0x20);
    CN1100_print("=============0x%x===========\n",value);

    /*
     *Get reset pin,and initialize the as an output
     *with value 0 to disable chip function
     *Maybe RESET PIN will not needed anymore
     */

    spidev->workqueue = create_singlethread_workqueue("spi_read_cn1100");
    if(spidev -> workqueue == NULL){
        CN1100_print("Unable to create workqueue\n");
        goto fail2;
    }

    INIT_WORK(&spidev->main,CN1100_FrameScanDoneInt_ISR);
    INIT_WORK(&spidev->reset_work,chm_ts_reset_func);

    /*Allocate an input device to report coordinate to android*/
    spidev->dev = input_allocate_device();
    if(!spidev->dev){
        CN1100_print("cannot get input device\n");
        goto fail3;
    }

    input_set_abs_params(spidev->dev,ABS_MT_TRACKING_ID,0,10,0,0);
    set_bit(EV_ABS, spidev->dev->evbit);                  
    set_bit(EV_KEY, spidev->dev->evbit);       
    set_bit(BTN_TOUCH, spidev->dev->keybit); //add

    
    set_bit(ABS_X, spidev->dev->absbit);
    set_bit(ABS_Y, spidev->dev->absbit); 
    set_bit(ABS_MT_SLOT, spidev->dev->absbit); 
    //set_bit(ABS_MT_TOUCH_MAJOR, spidev->dev->absbit);
    set_bit(ABS_MT_POSITION_X, spidev->dev->absbit);
    set_bit(ABS_MT_POSITION_Y, spidev->dev->absbit);   
 
        
    //set_bit(ABS_MT_WIDTH_MAJOR, spidev->dev->absbit);                     
    //set_bit(ABS_MT_TRACKING_ID,spidev->dev->absbit);
    CN1100_print("Max x %d Max y %d \n",screen_max_x,screen_max_y);
    input_set_abs_params(spidev->dev, ABS_MT_POSITION_X, 0, screen_max_x, 0, 0);  
    input_set_abs_params(spidev->dev, ABS_MT_POSITION_Y, 0, screen_max_y, 0, 0);  
    input_set_abs_params(spidev->dev, ABS_X, 0, screen_max_x, 0, 0);  
    input_set_abs_params(spidev->dev, ABS_Y, 0, screen_max_y, 0, 0);  
    //input_set_abs_params(spidev->dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
    //input_set_abs_params(spidev->dev, ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0); 

    input_mt_init_slots(spidev->dev,5 , INPUT_MT_DIRECT | INPUT_MT_DROP_UNUSED |
			    INPUT_MT_TRACK);
    spidev->dev->name = "tc1126_ts";
    spidev->dev->phys = "input/ts";
    spidev->dev->id.bustype = BUS_I2C;
    //spidev->dev->id.vendor = 0xDEAD;
    //spidev->dev->id.product = 0xBEEF;
    //spidev->dev->id.version = 0x0101;

    status = input_register_device(spidev->dev);
    if(status){
        CN1100_print("Cannot register input device\n");
        goto fail4;
    }	

    hrtimer_init(&spidev->systic,CLOCK_MONOTONIC,HRTIMER_MODE_REL);
    spidev->systic.function = CN1100_SysTick_ISR;

    get_chip_addr();
    if(spidev->i2c_ok){
        cn1100_init();
        update_cfg();
    }
    /*Related to specified hardware*/
    if(spidev->mode & CN1100_USE_IRQ){
        spidev->irq = gpio_to_irq(CN1100_INT_PIN); 
        //-     config_info.dev = &(spidev->dev->dev);
        //		status = input_request_int(&(config_info.input_type),cn1100_irq_handler,IRQF_TRIGGER_HIGH,spidev);
        status = request_irq(spidev->irq,cn1100_irq_handler,IRQF_TRIGGER_HIGH ,"cn1100-interrup", NULL);
        if (status) {
            CN1100_print( "tc1126_probe: request irq failed\n");
            goto fail6;
        }

    }

/*#ifdef DEBUG_PROC_USED
    spidev->chm_ts_proc = proc_create("tc1126_ts",0666,NULL,&chm_proc_fops);
    if(!spidev->chm_ts_proc){
        CN1100_print("Error to create debug entry\n");
    }
#endif*/


    hrtimer_start(&spidev->systic, ktime_set(0, SCAN_SYSTIC_INTERVAL), HRTIMER_MODE_REL);
    return 0;
fail6:
//        unregister_early_suspend(&spidev->early_suspend);
//fail5:
    input_unregister_device(spidev->dev);
fail4:
    input_free_device(spidev->dev);	
fail3:
    destroy_workqueue(spidev->workqueue);
fail2:
    kfree(spidev);
    return status;
}



static int chm_ts_remove(struct i2c_client *client)
{
    int status = 0;
    CN1100_print("driver is removing\n");
    remove_proc_entry("tc1126_ts", NULL);
#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&spidev->early_suspend);
#endif
    if(spidev->mode & (CN1100_USE_IRQ)){
        disable_irq_nosync(spidev->irq);
    }
    hrtimer_cancel(&spidev->systic);
    flush_workqueue(spidev->workqueue);

    kfree(bd);
    spidev->client = NULL;
    input_unregister_device(spidev->dev);
    destroy_workqueue(spidev->workqueue);
    if(spidev->mode & CN1100_USE_IRQ){
        free_irq(spidev->irq,NULL);
    }
    if(spidev){
        kfree(spidev);
    }
    return status;
}

int chm_ts_suspend(struct i2c_client *client,pm_message_t mesg)
{
    int ret = 0;
    spidev->mode |= CN1100_IS_SUSPENDED;
    hrtimer_cancel(&spidev->systic);
    flush_workqueue(spidev->workqueue);
    disable_irq_nosync(spidev->irq);
    ctp_wakeup(0);
    CN1100_print("suspend done\n");
    return ret;
}

int chm_ts_resume(struct i2c_client *client)
{
    int ret = 0;
    CN1100_print("%s\n",__func__);
    spidev->mode &= ~(CN1100_IS_SUSPENDED);
	ctp_wakeup(0);
	msleep(10);
    ctp_wakeup(1);

    get_chip_addr();
    msleep(10);
    if(spidev->i2c_ok){
        cn1100_init();
        update_cfg();
    }
    msleep(10);
    if(spidev->mode & CN1100_USE_IRQ){
        enable_irq(spidev->irq);
    }
    hrtimer_start(&spidev->systic, ktime_set(0, SCAN_SYSTIC_INTERVAL), HRTIMER_MODE_REL);

    return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void chm_ts_early_suspend(struct early_suspend *h){
    chm_ts_suspend(spidev->client,PMSG_SUSPEND);
}

static void chm_ts_late_resume(struct early_suspend *h){
    chm_ts_resume(spidev->client);
}
#endif

static const struct i2c_device_id chm_ts_id[] = {
    {"tc1126",0},
    {"cn1100",0},
    {},
};

MODULE_DEVICE_TABLE(i2c, chm_ts_id);
static int ctp_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    struct i2c_adapter *adapter = client->adapter;
    int ret; 

    if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
        return -ENODEV;

    if(twi_id == adapter->nr){
        pr_info("%s: addr= %x\n",__func__,client->addr);
        ret = 1;
        if(!ret){
            pr_info("%s:I2C connection might be something wrong \n",__func__);
            return -ENODEV;
        }else{           
            pr_info("I2C connection sucess!\n");
            strlcpy(info->type, "tc1126", I2C_NAME_SIZE);
            return 0;            
        }    

    }else{
        return -ENODEV;
    }    
}


static struct i2c_driver chm_ts_driver = {
    .class          = I2C_CLASS_HWMON,
    .probe		= chm_ts_probe,
    .remove		= chm_ts_remove,
    .id_table	= chm_ts_id,
    .address_list	= normal_i2c,
    .detect   = ctp_detect,
    .driver		= {
        .name	= "tc1126",
        .owner	= THIS_MODULE, 
    },
};
/*void ctp_print_info(struct ctp_config_info info)
{       
    CN1100_print("info.ctp_used:%d\n",info.ctp_used);
    CN1100_print("info.ctp_name:%s\n",info.name);
    CN1100_print("info.twi_id:%d\n",info.twi_id);
    CN1100_print("info.screen_max_x:%d\n",info.screen_max_x);
    CN1100_print("info.screen_max_y:%d\n",info.screen_max_y);
    CN1100_print("info.revert_x_flag:%d\n",info.revert_x_flag);
    CN1100_print("info.revert_y_flag:%d\n",info.revert_y_flag);
    CN1100_print("info.exchange_x_y_flag:%d\n",info.exchange_x_y_flag);
    CN1100_print("info.irq_gpio_number:%d\n",info.irq_gpio.gpio);
	config_info.wakeup_gpio.gpio=225;
    CN1100_print("info.wakeup_gpio_number:%d\n",info.wakeup_gpio.gpio);
} */   

/*static int ctp_get_system_config(void) //TODO Porting to device three nedded
{
    //ctp_print_info(config_info);

    twi_id = 0 ;//config_info.twi_id;
    screen_max_x = 800 ;//config_info.screen_max_x;
    SCREEN_HIGH = screen_max_x;
    screen_max_y = 480;//config_info.screen_max_y;
    SCREEN_WIDTH = screen_max_y;
    revert_x_flag = 1; //config_info.revert_x_flag;
    revert_y_flag = 1; //config_info.revert_y_flag;
    exchange_x_y_flag =1;//config_info.exchange_x_y_flag;
    CN1100_RESET_PIN= 225 ; //PH01
    if((screen_max_x == 0) || (screen_max_y == 0)){
        CN1100_print("%s:read config error!\n",__func__);
        return 0;
    }
    return 1;
}*/

/**
 * ctp_init_platform_resource - initialize platform related resource
 * return value: 0 : success
 *               -EIO :  i/o err.
 *
 */
static int init_platform_resource(void);
static int init_platform_resource()
{	
	int ret = -1;
    printk(KERN_INFO "tc1126 init_platform_resource\n");
	/*regulator_set_voltage(data->ctp_power_ldo,(int)(data->ctp_power_vol)*1000,(int)(data->ctp_power_vol)*1000);
	} else if(0 != data->ctp_power_io.gpio) {
		if(0 != gpio_request(data->ctp_power_io.gpio, NULL))
			pr_err("ctp_power_io gpio_request is failed,"
					"check if ctp independent power supply by gpio,"
					"ignore firstly\n");
		else
			gpio_direction_output(data->ctp_power_io.gpio, 1);
	}*/
	if(0 != gpio_request(225, NULL)) { 
		pr_err("wakeup gpio_request is failed\n");
		return ret;
	} 
	if (0 != gpio_direction_output(225, 1)) {
		pr_err("wakeup gpio set err!");
		return ret;
	}
     

	
	ret = 0;      
	return ret;
}
/*********************************CTP END***************************************/



static int __init cn1100_spi_init(void)
{
    int status = 0;
    printk(KERN_INFO "tc1126 cn1100_spi_init\n");
    bd = kmalloc(sizeof(bd_t),GFP_KERNEL);
    if(!bd){
        goto fail1;
    }

    spidev = kmalloc(sizeof(*spidev),GFP_KERNEL);
    if(!spidev){
        goto fail2;
    }

    spidev->mode &= ~(CN1100_DATA_PREPARED);

    init_waitqueue_head(&spidev->waitq);

    if (0 != init_platform_resource()) printk(KERN_INFO "tc1126 error init_platform_resource\n");

   /* if (!ctp_get_system_config()) {
        CN1100_print("%s:read config fail!\n",__func__);
        return status;
    }*/

    //	TODO:need confirm
    ctp_wakeup(1);

    msleep(20);
#ifdef BUILD_DATE
    CN1100_print("Build Time:%u\n",BUILD_DATE);
#endif
    status =  i2c_add_driver(&chm_ts_driver);
    if(status < 0){
        CN1100_print("Unable to add an i2c driver\n");
        goto fail3;
    }
    //i2c_new_device
    return 0;
fail3:
    kfree(spidev);
fail2:
    kfree(bd);
fail1:
    return status;
}

late_initcall(cn1100_spi_init);

static void __exit cn1100_spi_exit(void)
{
    i2c_del_driver(&chm_ts_driver);
    gpio_free(225);
    
}
module_exit(cn1100_spi_exit);

MODULE_DESCRIPTION("cn1100 spi transfer driver");
MODULE_LICENSE("GPL");
