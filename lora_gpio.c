#include "lora_gpio.h"
#include "debug.h"
#include "os_abstraction.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define GPIO_FS_PATH "/sys/class/gpio"

__attribute__((weak)) int gpio_dio0_callback(void)
{

    return 0;
}

static int s_write_file(char *path, char *content, int length)
{
    int fd = -1;
    int ret;

    fd = open(path, O_WRONLY);
    if (fd < 0) {
        // error
        print_errno("Open %s for write failed", path);
        return -1;
    }

    // export the gpio
    ret = write(fd, content, length);
    if (ret == -1) {
        //error
        print_errno("Write into file %s failed", path);
        close(fd);
        return -1;
    }

    close(fd);

    return ret;
}

int gpio_init(void)
{
    char fpath[255] = {0};
    char buf[10];
    int ret;

    // Configure RESET pin
    ret = sprintf(fpath, GPIO_FS_PATH "/gpio%d/value", RFM95_RESET);
    fpath[ret] = 0;

    do {
        if( access( fpath, F_OK ) != -1 ) {
            // file exists
            print_dbg("GPIO %d already exported", RFM95_RESET);
            break;
        } else {
            // file doesn't exist, export the GPIO and continue
            ret = sprintf(buf, "%d\n", RFM95_RESET);
            buf[ret] = 0;
            s_write_file(GPIO_FS_PATH "/export", buf, ret);
            print_dbg("GPIO %d exported", RFM95_RESET);
        }
    } while (1);

    // Output
    ret = sprintf(fpath, GPIO_FS_PATH "/gpio%d/direction", RFM95_RESET);
    fpath[ret] = 0;
    ret = sprintf(buf, "out\n");
    buf[ret] = 0;
    s_write_file(fpath, buf, ret);

    // TODO: configure DIO0 pin

    print_dbg("done!!!");
    return 0;
}

void gpio_deinit(void)
{
    char fpath[255] = {0};
    char buf[10];
    int ret;

    // Configure RESET pin
    ret = sprintf(fpath, GPIO_FS_PATH "/gpio%d/value", RFM95_RESET);
    fpath[ret] = 0;

    do {
        if( access( fpath, F_OK ) == -1 ) {
            // file doesn't exists
            print_dbg("GPIO %d already un-exported", RFM95_RESET);
            break;
        } else {
            // file exist, unexport the GPIO and continue
            ret = sprintf(buf, "%d\n", RFM95_RESET);
            buf[ret] = 0;
            s_write_file(GPIO_FS_PATH "/unexport", buf, ret + 1);
            print_dbg("GPIO %d un-exported", RFM95_RESET);
        }
    } while (1);

    print_dbg("done!!!");
}

int gpio_reset_rfm95(void)
{
    char fpath[255] = {0};
    char buf[10];
    int ret;

    ret = sprintf(fpath, GPIO_FS_PATH "/gpio%d/value", RFM95_RESET);
    fpath[ret] = 0;
    // RESET pin to LOW
    ret = sprintf(buf, "0\n");
    buf[ret] = 0;
    s_write_file(fpath, buf, ret);

    os_delay_ms(1);

    // RESET pin to HIGH
    ret = sprintf(buf, "1\n");
    buf[ret] = 0;
    s_write_file(fpath, buf, ret);

    os_delay_ms(10);

    print_inf("RFM95 reset DONE!");

    return 0;
}
