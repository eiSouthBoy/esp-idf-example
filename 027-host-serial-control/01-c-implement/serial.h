#ifndef __SERIAL_H__
#define __SERIAL_H__

#define SERIAL_DEVICE_PATH "/dev/ttyUSB0"
#define SERIAL_BAUD 115200


typedef struct
{
    
} serial_opt_t;


extern int open_serial(const char *device);
extern int close_serial(int fd);
extern void classic_reset(int fd);
extern void hard_reset(int fd);
extern int configure_serial(int fd, serial_opt_t option);
extern int send_data(int fd, const char *data, int len);
extern int recv_data(int fd, char *data, int size);


#endif