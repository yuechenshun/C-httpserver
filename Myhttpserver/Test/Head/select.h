#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <pthread.h>
#include <sys/time.h>

#define BUF_SIZE 1024

pthread_mutex_t mutex;

struct fdinfo
{
    int fd;
    int *maxfd;
    fd_set *readfd;
};

void *acceptConn(void *arg)
{
    fdinfo *info = (fdinfo*)arg;
    int cfd = accept(info->fd, NULL, NULL);
    if(cfd != -1)
    {
        printf("new client connect!\n");
    }
    pthread_mutex_lock(&mutex);
    FD_SET(cfd, info->readfd);
    pthread_mutex_unlock(&mutex);
    *(info->maxfd) = cfd > *(info->maxfd) ? cfd : *(info->maxfd);
    free(info);
    return NULL;
}

void *communication(void *arg)
{
    fdinfo *info = (fdinfo*)arg;
    char buf[BUF_SIZE];
    int readlen = read(info->fd, buf, BUF_SIZE);
    if(readlen == 0)
    {
        printf("client connect close!\n");
        pthread_mutex_lock(&mutex);
        FD_CLR(info->fd, info->readfd);
        pthread_mutex_unlock(&mutex);
        close(info->fd);
        free(info);
        return NULL;
    }
    else
    {
        printf("message from client is %s\n", buf);
        write(info->fd, buf, readlen);
    }
    free(info);
    return NULL;
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        printf("usage : %S <port>\n", argv[0]);
        exit(1);
    }

    pthread_mutex_init(&mutex, NULL);

    int lfd, cfd;
    struct sockaddr_in serv_addr, cli_addr;
    struct timeval timeout;
    // int readlen;

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

    fd_set readfd;
    FD_ZERO(&readfd);
    FD_SET(lfd, &readfd);
    int maxfd = lfd;

    while(1)
    {
        pthread_mutex_lock(&mutex);
        fd_set temp = readfd;
        pthread_mutex_unlock(&mutex);
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;
        int ret = select(maxfd + 1, &temp, NULL, NULL, &timeout);
        if(FD_ISSET(lfd, &temp))
        {
            // 将下列代码写入线程处理函数中即可
            // socklen_t cli_addrlen = sizeof(cli_addr);
            // int cfd = accept(lfd, (struct sockaddr *)&cli_addr, &cli_addrlen);
            // printf("start connect!, new client ip is %s, port is %d\n",
            //         inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
            // FD_SET(cfd, &readfd);
            // maxfd = cfd > maxfd ? cfd : maxfd;
            fdinfo *info = (fdinfo*)malloc(sizeof(fdinfo));
            info->fd = lfd;
            info->maxfd = &maxfd;
            info->readfd = &readfd;
            pthread_t tid;

            pthread_create(&tid, NULL, acceptConn, info);
            sleep(2);
            pthread_detach(tid);
        }
        for (int i = 0; i <= maxfd; ++i)
        {
            if(i != lfd && FD_ISSET(i, &temp))
            {
                // char buf[BUFSIZ];
                // readlen = read(i, buf, BUF_SIZE);
                // // 客户端关闭
                // if(readlen == 0)
                // {
                //     FD_CLR(i, &readfd);
                //     close(i);
                //     printf("closed client %d\n", i);
                // }
                // else
                // {
                //     printf("read message is : %s\n", buf);
                //     write(i, buf, readlen);
                // }
                fdinfo *info = (fdinfo*)malloc(sizeof(fdinfo));
                pthread_t tid;
                info->fd = i;
                info->readfd = &readfd;
                pthread_create(&tid, NULL, communication, info);
                pthread_detach(tid);
            }
        }
    }
    close(lfd);
    pthread_mutex_destroy(&mutex);
    return 0;
}