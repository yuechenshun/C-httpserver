#ifndef WEBFUNC_H
#define WEBFUNC_H

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
using namespace std;

// 错误处理函数
void unix_error(const char *err)
{
    perror(err);
    exit(0);
}

// 将文件描述符设置为非阻塞
static void setnonblocking(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

// 读取HTTP报文中的一行
int readline(int fd, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while (i < size - 1 && c != '\n')
    {
        n = recv(fd, &c, 1, 0);
        if (n == 1)
        {
            if (c == 'r')
            {
                n = recv(fd, &c, 1, MSG_PEEK);
                if (n == 1 && c == '\n')
                    recv(fd, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';
    return i;
}

int start_listen(char *port, int backlog)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0)
    {
        unix_error("socket()");
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(port));
    if (bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        unix_error("bind()");
    }
    if (listen(lfd, backlog) == -1)
    {
        unix_error("listen()");
    }
    printf("服务器已启动, IP:%s, 端口%d\n",
           inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
    return lfd;
}

#endif