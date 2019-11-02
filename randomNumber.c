#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/random.h>

#include "random_driver.h"


#define DRIVER_AUTHOR "<Dinh Tien_1712181>"
#define DRIVER_DESC "PROJECT 2"
#define DRIVER_VERSION "0.1"

static int i = 0;
static unsigned char randomNumber;
static char temp[4] = {'\0'};

typedef struct random_dev {
	unsigned char *control_regs;
	unsigned char *status_regs;
	unsigned char *data_regs;
} random_dev_t;

struct _random_drv {
	dev_t dev_num;
	struct class *dev_class;
	struct device *dev;
	random_dev_t *random_hw;
	struct cdev *vcdev;
	unsigned int open_cnt;
} random_drv;

/* _________ DEVICE _________ */
/* ham khoi tao thiet bi */
int random_hw_init(random_dev_t *hw)
{
        char *buf;
        buf = kzalloc(NUM_DEV_REGS *REG_SIZE, GFP_KERNEL);
        if (!buf) {
                return -ENOMEM;
        }

        hw->control_regs = buf;
        hw->status_regs = hw->control_regs + NUM_CTRL_REGS;
        hw->data_regs = hw->status_regs + NUM_STS_REGS;

        //Khoi ao gia tri ban dau cho cac thanh ghi
        hw->control_regs[CONTROL_ACCESS_REG] = 0X03;
        hw->status_regs[DEVICE_STATUS_REG] = 0x03;

        return 0;
}

/* ham giai phong thiet bi */
void random_hw_exit(random_dev_t *hw)
{
        kfree(hw->control_regs);
}

/* ham doc tu cac thanh ghi du lieu cua thiet bi */
int random_hw_read_data(random_dev_t *hw, int start_reg, int num_regs, char *kbuf)
{
	int read_bytes = num_regs;

	// kiem tra xem co quyen doc du lieu khong
	if ((hw->control_regs[CONTROL_ACCESS_REG] & CTRL_READ_DATA_BIT) == DISABLE)
		return -1;

	// kiem tra xem dia chi cua kernel buffer co hop le khong
	if (kbuf == NULL)
		return -1;

	// kiem tra xem vi tri cua cac thanh ghi can doc co hop ly khong
	if (start_reg > NUM_DATA_REGS)
		return -1;

	// dieu chinh lai so luong thanh ghi du lieu can doc (neu can thiet)
	if (num_regs > (NUM_DATA_REGS - start_reg)) 
		read_bytes = NUM_DATA_REGS - start_reg;

	// ghi du lieu tu kernel buffer vao cac thanh ghi du lieu
	memcpy(kbuf, hw->data_regs + start_reg, read_bytes);

	// cap nhat so lan doc tu cac thanh ghi du lieu
	hw->status_regs[READ_COUNT_L_REG] += 1;
	if (hw->status_regs[READ_COUNT_L_REG] == 0)
		hw->status_regs[READ_COUNT_L_REG] += 1;

	// tra ve so byte da doc duoc tu cac thanh ghi du lieu
	return read_bytes;
	
}

/* ham ghi vao cac thanh ghi du lieu cua thiet bi */
int random_hw_write_data(random_dev_t *hw, int start_reg, int num_regs, char *kbuf)
{
	int write_bytes = num_regs;

	// kiem tra xem co quyen ghi du lieu khong
	if ((hw->control_regs[CONTROL_ACCESS_REG] & CTRL_WRITE_DATA_BIT) == DISABLE)
		return -1;

	// kiem tra xem dia chi cua kernel buffer co hop le khong
	if (kbuf == NULL)
		return -1;

	// kiem tra xem vi tri cua cac thanh ghi can ghi co hop ly khong
	if (start_reg > NUM_DATA_REGS)
		return -1;

	// dieu chinh lai so luong thanh ghi du lieu can ghi (neu can thiet)
	if (num_regs > (NUM_DATA_REGS - start_reg)) {
		write_bytes = NUM_DATA_REGS - start_reg;
		hw->status_regs[DEVICE_STATUS_REG] |= STS_DATAREGS_OVERFLOW_BIT;
	}

	// doc di lieu tu cac thanh ghi du lieu vao kernel buffer
	memcpy(hw->data_regs + start_reg, kbuf, write_bytes);

	// cap nhat so lan ghi vao cac thanh ghi du lieu
	hw->status_regs[WRITE_COUNT_L_REG] += 1;
	if (hw->status_regs[WRITE_COUNT_L_REG] == 0)
		hw->status_regs[WRITE_COUNT_L_REG] += 1;

	// tra ve so byte da ghi vao thanh ghi du lieu
	return write_bytes;
}



/* _________ OS _________ */
/* cac ham entry points */
static int randNum_open(struct inode *inode, struct file *filp)
{
	random_drv.open_cnt++;
	printk("Handle opened envet (%d)\n", random_drv.open_cnt);
	return 0;
}

static int randNum_release(struct inode *inode, struct file *filp)
{
	printk("Handle closed event\n");
	return 0;
}

static ssize_t randNum_read(struct file *filp, char *buffer, size_t len, loff_t *off)
{
	i = 0;
	get_random_bytes(&randomNumber, sizeof(char));
	printk(KERN_INFO "RANDOMMACHINE: Random number is %d\n", randomNumber);
	if (len < 4) {
		printk(KERN_INFO "\nRANDOMMACHINE: Failed\n");
		return -EFAULT;
	}

	if (randomNumber != 0) {
		while (randomNumber != 0) {
			temp[i] = randomNumber % 10 + '0';
			randomNumber = randomNumber / 10;
			i++;
		}

		temp[i] = '\0';
		buffer[i] = '\0';
		i -= 1;
		while (i >= 0) {
			*buffer = temp[i];
			i -= 1;
			buffer += 1;
		}
		return 0;
	}
	else {
		*(buffer++) = '0';
		*buffer = '\0';
		return 0;
	}

}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = randNum_open,
	.release = randNum_release,
	.read = randNum_read,
};


static int __init RandNum_init(void)
{
	int ret = 0;
	random_drv.dev_num = 0;
	ret = alloc_chrdev_region(&random_drv.dev_num, 0, 1, "Random Number");
	if (ret < 0) {
		printk("Failed to register device number dynamically\n");
		goto failed_register_devnum;
	}
	
	printk("Allocated device number (%d, %d)\n", MAJOR(random_drv.dev_num), MINOR(random_drv.dev_num));
	
	/* tao device file */
	random_drv.dev_class = class_create(THIS_MODULE, "class_random_dev");
	if (random_drv.dev_class == NULL) {
		printk("Failed to create a device class\n");
		goto failed_create_class;
	}

	random_drv.dev = device_create(random_drv.dev_class, NULL, random_drv.dev_num, NULL, "random_dev");
	if (IS_ERR(random_drv.dev)) {
		printk("Failed to create a device\n");
		goto failed_create_device;
	}

	/* cap phat bo nho cho cac cau truc du lieu cua driver va khoi tao */
	random_drv.random_hw = kzalloc(sizeof(random_dev_t), GFP_KERNEL);
	if (!random_drv.random_hw) {
		printk("Failed to alocate data structure of the driver\n");
		ret = -ENOMEM;
		goto failed_allocate_structure;
	}

	/* khoi tao thiet bi vat ly */
	ret = random_hw_init(random_drv.random_hw);
	if (ret < 0) {
		printk("Failed to initialize a virtual character device\n");
		goto failed_init_hw;
	}


	/* dang ky cac entry point voi kernel */
	random_drv.vcdev = cdev_alloc();
	if (random_drv.vcdev == NULL) {
		printk("Failed to allocate cdev structure\n");
			goto failed_allocate_cdev;
	}
	cdev_init(random_drv.vcdev, &fops);
	ret = cdev_add(random_drv.vcdev, random_drv.dev_num, 1);
	if (ret < 0) {
		printk("Failed to add a char device to the system\n");
		goto failed_allocate_cdev;
	}


	printk("Initialize successfully\n");
	return 0;

failed_allocate_cdev:
	random_hw_exit(random_drv.random_hw);
failed_init_hw:
	kfree(random_drv.random_hw);
failed_allocate_structure:
	device_destroy(random_drv.dev_class, random_drv.dev_num);
failed_create_device:
	class_destroy(random_drv.dev_class);
failed_create_class:
	unregister_chrdev_region(random_drv.dev_num, 1);
failed_register_devnum:
	return ret;
}

static void __exit RandNum_exit(void)
{
	/*huy dang ky entry point voi kernel */
	cdev_del(random_drv.vcdev);

	/* giai phong thiet bi vat ly */
	random_hw_exit(random_drv.random_hw);
	
	/* giai phong bo nho da cap phat cu truc du lieu cua driver */
	kfree(random_drv.random_hw);

	/* xoa bo nho device file */
	device_destroy(random_drv.dev_class, random_drv.dev_num);
	class_destroy(random_drv.dev_class);

	/* giai phong device number */
	unregister_chrdev_region(random_drv.dev_num, 1);
	printk("___Exit___\n");
}

module_init(RandNum_init);
module_exit(RandNum_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_SUPPORTED_DEVICE("testdevie");
