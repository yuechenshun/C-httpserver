#include "header.h"

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("使用 <端口号>\n");
        exit(0);
    }
    // 建立监听
    int lfd, cfd;
    struct sockaddr_in cli_addr;
    lfd = start_listen(argv[1], 10);
    // 初始化线程池
    ThreadPool<Http> *threadpool = new ThreadPool<Http>(3, 100);
    // 创建epoll实例
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        unix_error("epoll_create()");
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    // lfd上树
    epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    // 循环监听消息
    while (1)
    {
        struct epoll_event evs[1024];
        int size = sizeof(evs) / sizeof(evs[0]);
        int nums = epoll_wait(epfd, evs, size, -1);
        printf("变化的文件描述符个数为: %d\n", nums);
        for (int i = 0; i < nums; ++i)
        {
            int fd = evs[i].data.fd;
            // 判读是否属于用于监听的文件描述符
            if (fd == lfd)
            {
                socklen_t cli_len = sizeof(cli_addr);
                int cfd = accept(fd, (struct sockaddr *)&cli_addr, &cli_len);
                setnonblocking(cfd);
                printf("有新的连接到来，ip为%s, 端口为%d\n",
                       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            }
            else
            {
                Http *http = new Http(epfd, &evs[i]);
                threadpool->add(http);
            }
        }
    }
    close(lfd);

    return 0;
}