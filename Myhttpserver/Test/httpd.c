#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define ISSPACE(x) isspace((int)(x))

#define SERVER_STRING "Server: myhttpd/0.1.0\r\n"

void error_handling(char *error);
void setnoblock(int fd);
int readline(int fd, char *buf, int size);
void *Communication(void *arg);
void acceptConn(int epfd, struct epoll_event *ev);
void sendHeader(int client);
void notFound(int client);
void sendFile(int client, char *path);

typedef struct fdinfo
{
    int epfd;
    struct epoll_event *ev;
}fdinfo;

// 错误处理函数
void error_handling(char *error)
{
    perror(error);
    exit(1);
}

// 设置非阻塞
void setnoblock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

// 读取一行
int readline(int fd, char *buf, int size)
{
    int i = 0;
    char c = '\0'; // ASCII码第一个，即NULL
    int n;
    while (i < size - 1 && c != '\n')
    {
        n = recv(fd, &c, 1, 0); // 每次读取一个字符
        if (n == 1)
        {
            if (c == '\r')
            {
                n = recv(fd, &c, 1, MSG_PEEK);
                if (n > 0 && c == '\n')
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

// 多线程处理函数
void *Communication(void *arg)
{
    printf("子线程开始处理客户请求,线程id为%ld\n", pthread_self());
    fdinfo *info = (fdinfo *)arg;
    int epfd = info->epfd;
    struct epoll_event *ev = info->ev;
    char buf[1024] = "";     // 存放请求行
    char temp[1024] = "";    // 存放请求头
    char method[128] = "";   // 存放方法
    char url[256] = "";      // 存放URL
    char content[512] = ""; // 存放请求内容
    char path[512] = "";     // 请求文件路径
    // 读取请求行
    int n = readline(ev->data.fd, buf, sizeof(buf));
    if (n <= 0)
    {
        printf("读取错误或者连接已关闭\n");
        epoll_ctl(epfd, EPOLL_CTL_DEL, ev->data.fd, ev);
        close(ev->data.fd);
        return NULL;
    }
    printf("请求行内容为:%s\n", buf);
    // 循环读取请求头每一行并丢弃
    int ret = 0;
    while ((ret = readline(ev->data.fd, temp, sizeof(temp))) > 0)
        ;
    printf("请求头读取完毕\n");
    // 解析请求行，判断是否为GET请求 GET /a.txt HTTP/1.1\r\n
    int i = 0, j = 0;
    while (!ISSPACE(buf[i]) && i < sizeof(buf) - 1)
    {
        method[i] = buf[i];
        i++;
    }
    // 如果不是GET请求,退出程序
    if (strcasecmp(method, "GET"))
    {
        printf("非GET请求，请求无效!\n");
        return NULL;
    }
    j = i;
    // 跳过空格
    while (ISSPACE(buf[j]) && j < n)
        j++;
    i = 0;
    while (!ISSPACE(buf[j]) && i < sizeof(url) - 1 && j < n)
    {
        url[i++] = buf[j++];
    }
    printf("客户请求的url为%s\n", url);
    // 如果没有请求文件,默认请求当前路径
    char *query_str = url + 1;
    if (*query_str == 0)
    {
        query_str = "./";
    }
    /* 判断url中请求目录是否存在*/
    // 获取当前程序工作路径, 并拷贝至path中, 最后切换至该目录
    char *p = getenv("PWD");
    strcpy(path, p);
    strcat(path, "/htdocs");
    strcat(path, url);
    chdir(path);

    printf("请求文件所在路径为%s\n", path);
    // 判断文件是否存在
    struct stat s;
    // 请求文件不存在
    if (stat(path, &s) < 0)
    {
        printf("请求文件不存在!\n");
        notFound(ev->data.fd);
    }
    else
    {
        // 请求普通文件
        if (S_ISREG(s.st_mode))
        {
            printf("请求普通文件\n");
            // 发送 200 OK
            sendHeader(ev->data.fd);
            sendFile(ev->data.fd, path);
        }
        // 请求目录
        else if (S_ISDIR(s.st_mode))
        {
            printf("请求当前目录\n");
        }
    }
    free(info);
    return NULL;
}

// 不建立子线程处理客户端请求
void acceptConn(int epfd, struct epoll_event *ev)
{
    char buf[1024] = "";     // 存放请求行
    char temp[1024] = "";    // 存放请求头
    char method[128] = "";   // 存放方法
    char url[256] = "";      // 存放URL
    char content[512] = ""; // 存放请求内容
    char path[512] = "";     // 请求文件路径
    // 读取请求行
    int n = readline(ev->data.fd, buf, sizeof(buf));
    if (n <= 0)
    {
        printf("读取错误或者连接已关闭\n");
        epoll_ctl(epfd, EPOLL_CTL_DEL, ev->data.fd, ev);
        close(ev->data.fd);
        return;
    }
    printf("请求行内容为:%s\n", buf);
    // 循环读取请求头每一行并丢弃
    int ret = 0;
    while ((ret = readline(ev->data.fd, temp, sizeof(temp))) > 0)
        ;
    printf("请求头读取完毕\n");
    // 解析请求行，判断是否为GET请求 GET /a.txt HTTP/1.1\r\n
    int i = 0, j = 0;
    while (!ISSPACE(buf[i]) && i < sizeof(buf) - 1)
    {
        method[i] = buf[i];
        i++;
    }
    // 如果不是GET请求,退出程序
    if (strcasecmp(method, "GET"))
    {
        printf("非GET请求，请求无效!\n");
        return;
    }
    j = i;
    // 跳过空格
    while (ISSPACE(buf[j]) && j < n)
        j++;
    i = 0;
    while (!ISSPACE(buf[j]) && i < sizeof(url) - 1 && j < n)
    {
        url[i++] = buf[j++];
    }
    printf("客户请求的url为%s\n", url);
    // 如果没有请求文件,默认请求当前路径
    char *query_str = url + 1;
    if (*query_str == 0)
    {
        query_str = "./";
    }
    /* 判断url中请求目录是否存在*/
    // 获取当前程序工作路径, 并拷贝至path中, 最后切换至该目录
    char *p = getenv("PWD");
    strcpy(path, p);
    strcat(path, "/htdocs");
    strcat(path, url);
    chdir(path);

    printf("请求文件所在路径为%s\n", path);
    // 判断文件是否存在
    struct stat s;
    // 请求文件不存在
    if (stat(path, &s) < 0)
    {
        printf("请求文件不存在!\n");
        notFound(ev->data.fd);
    }
    else
    {
        // 请求普通文件
        if (S_ISREG(s.st_mode))
        {
            printf("请求普通文件\n");
            // 发送 200 OK
            sendHeader(ev->data.fd);
            sendFile(ev->data.fd, path);
        }
        // 请求目录
        else if (S_ISDIR(s.st_mode))
        {
            printf("请求当前目录\n");
        }
    }
    return;
}

// 处理200 OK
void sendHeader(int client)
{
    char buf[1024] = "";
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

// 处理404 NOT FOUND
void notFound(int client)
{
    char buf[1024] = "";
    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

// 发送文件
void sendFile(int client, char *path)
{
    // 获取文件的文件描述符
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        error_handling("open()");
    }
    char buf[1024] = "";
    int len = 0;
    while (1)
    {
        len = read(fd, buf, sizeof(buf));
        if (len < 0)
        {
            error_handling("read()");
        }
        else if(len == 0)
        {
            printf("文件内容为空\n");
            break;
        }
        else
        {
            send(client, buf, len, 0);
            printf("文件发送成功!\n");
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("使用 %s<端口号> \n", argv[0]);
        exit(1);
    }
    int lfd, cfd;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t newthread;
    // 创建套接字
    lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        error_handling("socket()");
    }
    // 绑定
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    if (bind(lfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        error_handling("bind()");
    }
    // 监听
    if (listen(lfd, 10) == -1)
    {
        error_handling("listen()");
    }
    printf("服务器已启动, IP:%s, 端口%d\n",
           inet_ntoa(serv_addr.sin_addr), ntohs(serv_addr.sin_port));
    //创建红黑树
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        error_handling("epoll_create()");
    }
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    // lfd 上树
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
            // 判断是否为用于监听的文件描述符
            if (fd == lfd)
            {
                socklen_t cli_len = sizeof(cli_addr);
                int cfd = accept(fd, (struct sockaddr *)&cli_addr, &cli_len);
                setnoblock(cfd); // 将文件描述符设置为非阻塞
                printf("有新的连接到来，ip为%s, 端口为%d\n",
                       inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            }
            else
            {
                // http服务器处理
                fdinfo *info = (fdinfo*)malloc(sizeof(fdinfo));
                info->epfd = epfd;
                info->ev = &evs[i];
                pthread_create(&newthread, NULL, Communication, info);
                pthread_detach(newthread);
                // acceptConn(epfd, &evs[i]);  /*不启动多线程*/
            }
        }
    }
    close(lfd);
    return 0;
}