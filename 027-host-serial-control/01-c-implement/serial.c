#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "serial.h"

// 定义 DTR 和 RTS 信号
#define TIOCM_DTR 0x002
#define TIOCM_RTS 0x004

// 设置 DTR 和 RTS 信号
static void set_dtr(int fd, int level)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    if (level)
    {
        status |= TIOCM_DTR;
    }
    else
    {
        status &= ~TIOCM_DTR;
    }
    ioctl(fd, TIOCMSET, &status);
}

static void set_rts(int fd, int level)
{
    int status;
    ioctl(fd, TIOCMGET, &status);
    if (level)
    {
        status |= TIOCM_RTS;
    }
    else
    {
        status &= ~TIOCM_RTS;
    }
    ioctl(fd, TIOCMSET, &status);
}

// 设备进入boot
void classic_reset(int fd)
{
    set_dtr(fd, 0);
    set_rts(fd, 1);
    usleep(100000);
    set_dtr(fd, 1);
    set_rts(fd, 0);
    usleep(50000);
    set_dtr(fd, 0);
}

// 设备进入重启
void hard_reset(int fd)
{
    set_dtr(fd, 0);
    set_rts(fd, 1);
    usleep(200000);
    set_rts(fd, 0);
    usleep(200000);
}

int open_serial(const char *device)
{
    int fd = 0;
    fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)
        printf("open serial: Unable to open %s\n", device);
    else
    {
        // fcntl(fd, F_SETFL, 0); // default blcck mode
        fcntl(fd, F_SETFL, O_NONBLOCK);
    }
    return (fd);
}

int close_serial(int fd)
{
    return close(fd);
}

int configure_serial(int fd, serial_opt_t option)
{
    int rc = 0;
    struct termios opt;
    rc = tcgetattr(fd, &opt);
    if (0 != 0)
    {
        printf("get attr: Unable to get\n");
        return rc;
    }

    // 波特率
    cfsetospeed(&opt, B115200);
    cfsetispeed(&opt, B115200);

    opt.c_cflag |= (CLOCAL | CREAD); //(本地连接（不改变端口所有者)|接收使能)
    opt.c_cflag &= ~CSIZE; // 屏蔽字符大小位
    opt.c_cflag |= CS8; // 数据位（8位）
    opt.c_cflag &= ~PARENB; // 校验位（无校验）
    opt.c_cflag &= ~CSTOPB; // 停止位（无停止）
    opt.c_cflag &= ~CRTSCTS; // 流控（禁用流控）

    tcflush(fd, TCIOFLUSH);
    rc = tcsetattr(fd, TCSANOW, &opt);
    if (rc != 0)
    {
        printf("set attr: Unable to set\n");
        return rc;
    }

    return 0;
}

int send_data(int fd, const char *data, int len)
{
    int write_byte = write(fd, data, len);
    return write_byte;
}

int recv_data(int fd, char *data, int size)
{
    int read_byte = 0; 
    read_byte = read(fd, data, size);
    return read_byte;
}