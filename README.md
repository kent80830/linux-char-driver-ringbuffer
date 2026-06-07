# linux-char-driver-ringbuffer

# 环形缓冲区字符设备驱动

Linux内核环形缓冲区字符设备驱动，实现生产者-消费者模型。

## 功能

- 4KB环形缓冲区
- 阻塞读写（空时读阻塞，满时写阻塞）
- 多进程并发测试

## 编译

```bash
make
gcc test_app.c -o test_app
