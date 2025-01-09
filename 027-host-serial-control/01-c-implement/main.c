#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "serial.h"

// 线程同步工具
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_not_full = PTHREAD_COND_INITIALIZER;

// 运行标志
volatile sig_atomic_t running = 1;

void cleanup(void) 
{
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_not_empty);
    pthread_cond_destroy(&cond_not_full);
}

void signal_handler(int sig) 
{
    if (sig == SIGINT) 
    {
        printf("Caught signal %d\n", sig);
        running = 0; // 设置停止标志
    }
}

void print_lines(const char *str) 
{
    const char *start = str; // 指向每行开始的位置
    while (*str) {
        if (*str == '\n') {
            // 打印从上次换行后到当前换行前的内容
            fwrite(start, 1, str - start, stdout);
            putchar('\n'); // 打印换行符

            // 移动到下一行的开始位置
            start = str + 1;
        }
        str++;
    }

    // 如果最后一行没有以换行符结束，则打印最后一行
    if (start != str) {
        fwrite(start, 1, str - start, stdout);
        if (str - start > 0 && *(str - 1) != '\n') {
            putchar('\n'); // 如果最后一行不是以换行符结束，手动添加换行符
        }
    }
}

/* 时间精确到ms */
static int lib_system_datetime_ms_string_get(char datetime[256])
{
    struct tm *tm_t;
    struct timeval time;
    gettimeofday(&time,NULL);
    tm_t = localtime(&time.tv_sec);
    if(NULL != tm_t) 
    {
        snprintf(datetime, 50, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
            tm_t->tm_year+1900,
            tm_t->tm_mon+1, 
            tm_t->tm_mday,
            tm_t->tm_hour, 
            tm_t->tm_min, 
            tm_t->tm_sec,
            time.tv_usec/1000);
    }
    return 0;
}

void* writer_thread(void *arg) 
{
    int fd = *(int *)arg;
    char buffer[1024] = {0};

    while (running) 
    {
       if (NULL != fgets(buffer, sizeof(buffer), stdin))
       {
            send_data(fd, buffer, strlen(buffer));
            printf("send data: %s\n", buffer);
       }
    }
    return NULL;
}

void* reader_thread(void *arg) 
{
    int fd = *(int *)arg;
    int byte_read = 0;
    char buffer[1024] = {0};
    while (running) 
    {

       byte_read = recv_data(fd, buffer, sizeof(buffer) - 1);
       if (byte_read > 0)
       {
            char time_txt[256] = {0};
            lib_system_datetime_ms_string_get(time_txt);
            buffer[byte_read] = '\0';
            printf("%s recv data(%d): %s", time_txt, byte_read, buffer);
       }
       usleep(100000);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int fd = 0, rc = 0;
    signal(SIGINT, signal_handler);
    fd = open_serial(SERIAL_DEVICE_PATH);
    if (fd < 0)
    {
        return 1;
    }
    serial_opt_t options;
    rc = configure_serial(fd, options);
    if (0 != rc)
    {
        goto exit;
    }

    pthread_t tid_writer, tid_reader;
    pthread_create(&tid_reader, NULL, reader_thread, (void *)&fd);
    pthread_create(&tid_writer, NULL, writer_thread, (void *)&fd);

    pthread_join(tid_writer, NULL);
    pthread_join(tid_reader, NULL);

    cleanup();

exit:
    if (fd >= 0)
        close_serial(fd);
    return 0;
}