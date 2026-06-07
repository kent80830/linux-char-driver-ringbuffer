#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>

#define DEV_NODE "/dev/ringbuf0"
#define WRITE_CONTENT "exp_ringbuf_test_data_"

int exit_flag = 1;

void sig_exit(int sig){exit_flag = 0;}

void producer_task(void)
{
    int fd = open(DEV_NODE, O_WRONLY);
    if(fd < 0){perror("open write dev fail");exit(1);}
    char send_buf[128];
    int i;

    for(i = 0; i < 10 && exit_flag; i++){
        sprintf(send_buf,"%s%d\n",WRITE_CONTENT,i);
        write(fd, send_buf, strlen(send_buf));
        printf("[生产者] 写入：%s", send_buf);
        sleep(1);
    }
    close(fd);
}

void consumer_task(void)
{
    int fd = open(DEV_NODE, O_RDONLY);
    if(fd < 0){perror("open read dev fail");exit(1);}
    char recv_buf[128] = {0};
    int recv_len;

    while(exit_flag){
        recv_len = read(fd, recv_buf, sizeof(recv_buf)-1);
        if(recv_len > 0){
            printf("[消费者] 读到：%.*s\n", recv_len, recv_buf);
            memset(recv_buf,0,sizeof(recv_buf));
        }
    }
    close(fd);
}

int main(void)
{
    signal(SIGINT, sig_exit);

    pid_t pid = fork();
    if(pid == 0){
        consumer_task();
    }else if(pid > 0){
        producer_task();
        wait(NULL);
    }else{
        perror("fork create process fail");
        return -1;
    }
    return 0;
}
