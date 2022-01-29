#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("usage : %S <port>\n", argv[0]);
        exit(1);
    }

    int lfd, cfd;
    struct sockaddr_in serv_addr, cli_addr;

    lfd = socket(PF_INET, SOCK_STREAM, 0);
    if(lfd == -1)
    {
        perror("socket()");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(lfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
    {
        perror("bind()");
        exit(1);
    }
    if(listen(lfd, 10) == -1)
    {
        perror("listen()");
        exit(1);
    }

    // 创建epoll实例
    int epfd = epoll_create(1);
    if(epfd == -1)
    {
        perror("epoll_create()");
        exit(-1);
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;

    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);

    while(1)
    {
        struct epoll_event evs[1024];
        int size = sizeof(evs) / sizeof(evs[0]);
        int num = epoll_wait(epfd, evs, size, -1);
        printf("num is %d\n", num);
        for (int i = 0; i < num; i++)
        {
            int fd = evs[i].data.fd;
            // 判断是否为监听的文件描述符
            if(fd == lfd)
            {
                cfd = accept(fd, NULL, NULL);
                // struct epoll_event ev;
                ev.events = EPOLLIN;
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            }
            else
            {
                char buf[BUF_SIZE];
                int readlen = read(fd, buf, BUF_SIZE);
                if(readlen == 0)
                {
                    printf("client connect close!\n");
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                }
                else
                {
                    printf("read message is : %s\n", buf);
                    write(fd, buf, readlen);
                }
            }
        }
    }
    close(lfd);
    return 0;
}