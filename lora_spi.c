#include "lora_spi.h"
#include "debug.h"
#include "os_abstraction.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <unistd.h>

static int s_spi_fd = -1;

int lora_spi_init(void)
{
    uint32_t mode;
    uint8_t bits = 8;
    int ret;

    s_spi_fd = open(SPI_DEV, O_RDWR);
    if (s_spi_fd < 1) {
        print_errno("Can't open " SPI_DEV ".");
        return -1;
    }

    // spi mode
    mode = 0;
    ret = ioctl(s_spi_fd, SPI_IOC_WR_MODE32, &mode);
    if (ret == -1) {
        print_errno("can't set spi mode");
    }

    ret = ioctl(s_spi_fd, SPI_IOC_RD_MODE32, &mode);
    if (ret == -1) {
        print_errno("can't get spi mode");
    }

    // bits per word
    bits = 8;
    ret = ioctl(s_spi_fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1) {
        print_errno("can't set bits per word");
    }

    ret = ioctl(s_spi_fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1) {
        print_errno("can't get bits per word");
    }

    // max speed hz
    mode = 8000000;
    ret = ioctl(s_spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &mode);
    if (ret == -1) {
        print_errno("can't set max speed hz");
    }

    ret = ioctl(s_spi_fd, SPI_IOC_RD_MAX_SPEED_HZ, &mode);
    if (ret == -1) {
        print_errno("can't get max speed hz");
    }

    return 0;
}

void lora_spi_deinit(void)
{
    if(s_spi_fd > 0) {
        close(s_spi_fd);
        s_spi_fd = -1;
    }
}

int lora_spi_transfer1(uint8_t addr, uint8_t value)
{
    int ret;
    uint8_t tx[2];
    uint8_t rx[2];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)&tx,
        .rx_buf = (unsigned long)&rx,
        .len = 2,
        .delay_usecs = 0,
        .speed_hz = 8000000,
        .bits_per_word = 8,
    };

    tx[0] = addr;
    tx[1] = value;

    ret = ioctl(s_spi_fd, SPI_IOC_MESSAGE(1), &tr);
    if (ret < 1) {
        print_errno("can't send spi message");
        return -1;
    }

    os_delay_ms(1);

    return rx[1];
}

int lora_spi_transfer(uint8_t addr, uint8_t *txbuf, uint8_t *rxbuf, int length)
{
    int ret;

    struct spi_ioc_transfer tr[2] = {
        {
            .tx_buf = (unsigned long)&addr,
            .rx_buf = (unsigned long)NULL,
            .len = 1,
            .delay_usecs = 0,
            .speed_hz = 8000000,
            .bits_per_word = 8,
        },
        {
            .tx_buf = (unsigned long)&txbuf,
            .rx_buf = (unsigned long)&rxbuf,
            .len = length,
            .delay_usecs = 0,
            .speed_hz = 8000000,
            .bits_per_word = 8,
        }
    };

    ret = ioctl(s_spi_fd, SPI_IOC_MESSAGE(2), tr);
    if (ret < 1) {
        print_errno("can't send spi messages");
        return -1;
    }

    return 0;
}

int lora_spi_read_reg(uint8_t addr)
{
    return lora_spi_transfer1(addr & 0x7F, 0x00);
}

int lora_spi_write_reg(uint8_t addr, uint8_t value)
{
    return lora_spi_transfer1(addr | 0x80, value);
}

uint8_t readRegister(uint8_t address)
{
    return lora_spi_transfer1(address & 0x7F, 0x00);
}
void writeRegister(uint8_t address, uint8_t value)
{
    // print_dbg("addr %x = %x", address, value);
    lora_spi_transfer1(address | 0x80, value);
}
uint8_t singleTransfer(uint8_t address, uint8_t value)
{
    return lora_spi_transfer1(address, value);
}
