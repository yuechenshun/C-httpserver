#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define BUFFSIZE 1024

// 设置非阻塞
void setnoblock(int fd)
{
    int flag = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

int main(int agrc, char *argv[])
{
    int sock;
    char message[BUFFSIZE];
    int str_len;
    struct sockaddr_in serv_addr;

    if (agrc != 3)
    {
        printf("使用: %s <IP> <端口号>\n", argv[0]);
        exit(1);
    }
    //printf("debug..\n");
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        printf("socket() error !\n");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    // printf("debug..\n");
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        printf("connect() error !\n");
        exit(1);
    }
    else
    {
        //printf("debug\n");
        puts("连接成功 !");
    }
    
    setnoblock(sock);
    while (1)
    {
        printf("请输入要发送的信息，按q退出:");
        fgets(message, BUFFSIZE, stdin);

        if (!strcmp(message, "q\n") || !strcmp(message, "Q\n"))
        {
            break;
        }
        write(sock, message, BUFFSIZE);
        str_len = read(sock, message, BUFFSIZE - 1);
        message[str_len] = 0;
        printf("来自服务器的信息为: %s", message);
    }
    close(sock);
    return 0;
}