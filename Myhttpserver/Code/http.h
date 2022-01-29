#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <ctype.h>
#include <dirent.h>
#include "webfunc.h"
using namespace std;

#define ISSPACE(x) isspace((int)(x))
const char *SERVER_STRING = "Server: yuechenshun's http/1.0\r\n";
char File[1024] = "";

class Http
{
public:
    Http(int epfd, struct epoll_event *ev) : epfd(epfd), ev(ev) {}
    ~Http() {}

    void work();
    inline char *getRequestline();
    inline int getcfd();

private:
    void prase_request();
    void handle_request();
    void sendHeader(int cfd, char *filetype);
    void sendFile(int cfd, char *path);
    void badRequest(int cfd);
    void notFound(int cfd);
    void closeConn(int cfd);
    char *getType(char *path);
    char *cat(char *path, const char *filename);

private:
    char request_line[512] = ""; // 存放请求行
    char method[128] = "";       // 存放方法名
    char url[512] = "";          // 存放URL
    char protocol[128] = "";     // 存放协议
    char content[1024] = "";     // 存放请求内容
    char path[512] = "";         // 存放请求文件路径
    char temp[512] = "";         // 存放临时内容

private:
    int epfd;               // 红黑树根节点
    struct epoll_event *ev; // 事件
};

// Http服务器工作函数
void Http::work()
{
    prase_request();
    handle_request();
}

// 获取请求行内容
inline char *Http::getRequestline()
{
    return request_line;
}

// 获取当前通信文件描述符
inline int Http::getcfd()
{
    return ev->data.fd;
}

// 解析http请求
void Http::prase_request()
{
    int cfd = ev->data.fd;
    int n = readline(cfd, request_line, sizeof(request_line));
    if (n < 0)
    {
        printf("读取错误或客户端已经关闭了连接\n");
        closeConn(cfd);
        return;
    }
    printf("请求行内容为: %s\n", request_line);
    // 循环读取请求体并丢弃
    int ret = 0;
    while ((ret = readline(cfd, temp, sizeof(temp))) > 0)
        ;
    // 解析请求行，将方法、URL、协议存储到对应数组中
    int i = 0, j = 0;
    while (!ISSPACE(request_line[i]) && i < sizeof(method) - 1)
    {
        method[i] = request_line[i];
        i++;
    }
    printf("请求使用的方法为: %s\n", method);
    j = i;
    while (ISSPACE(request_line[j]) && j < n)
        j++;
    i = 0;
    while (!ISSPACE(request_line[j]) && i < sizeof(url) - 1 && j < n)
    {
        url[i++] = request_line[j++];
    }
    printf("请求的URL为: %s\n", url);
    i = 0;
    while (ISSPACE(request_line[j]) && j < n)
        j++;
    while (!ISSPACE(request_line[j]) && i < sizeof(protocol) - 1 && request_line[j] != 'r' && j < n)
    {
        protocol[i++] = request_line[j++];
    }
    printf("请求使用协议为: %s\n", protocol);
}

// 处理http请求
void Http::handle_request()
{
    int cfd = ev->data.fd;
    // 判断请求是否合理, 如果不合理则发送400状态码
    if (method == "" || strcasecmp(method, "GET"))
    {
        printf("请求方法错误,请求无效!\n");
        badRequest(cfd);
        closeConn(cfd);
        return;
    }
    // 解析URL,如果没有指定请求文件,则指定当前工作目录
    char *query_str = url + 1;
    if (*query_str == 0)
    {
        char *root = (char *)"/";
        strcpy(url, root);
    }
    // 获取当前工作路径, 拷贝至path中, 并与url拼接
    char *curpath = getenv("PWD");
    strcpy(path, curpath);
    strcat(path, "/htdocs");
    strcat(path, url);
    chdir(path);

    printf("当前客户请求路径为: %s\n", path);
    // 判断文件是否存在
    struct stat s;
    // 请求文件不存在
    if (stat(path, &s) < 0)
    {
        printf("客户请求文件不存在!\n");
        notFound(cfd);
        closeConn(cfd);
        return;
    }
    else
    {
        // 请求的是普通文件
        if (S_ISREG(s.st_mode))
        {
            printf("客户请求普通文件\n");
            sendHeader(cfd, getType(path));
            sendFile(cfd, path);
            closeConn(cfd);
        }
        // 请求的是当前目录
        else if (S_ISDIR(s.st_mode))
        {
            printf("客户请求当前目录\n");
            sendHeader(cfd, getType(path));
            cat(path, "/head.html");
            sendFile(cfd, File);
            // 获取当前目录下的所有文件
            struct dirent **list = NULL;
            int num = scandir(path, &list, NULL, alphasort);
            if (num == -1)
            {
                printf("获取当前目录下的文件失败!\n");
                return;
            }
            char buf[1024] = "";
            for (int i = 0; i < num; i++)
            {
                if (list[i]->d_name == "." || list[i]->d_name == ".." 
                || list[i]->d_name == "head.html" || list[i]->d_name == "tail.html")
                {
                    free(list[i]);
                    continue;
                }
                else
                {
                    sprintf(buf, "<li><a href=%s>%s</a></li>",
                            list[i]->d_name, list[i]->d_name);
                    send(cfd, buf, sizeof(buf), 0);
                    free(list[i]);
                }
            }
            free(list);
            cat(path, "/tail.html");
            sendFile(cfd, File);
            closeConn(cfd);
        }
    }
    return;
}

// 请求成功，发送200 OK
void Http::sendHeader(int cfd, char *filetype)
{
    char buf[1024] = "";
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type:%s\r\n", filetype);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, strlen(buf), 0);
}

// 请求成功，想客户端套接字发送文件
void Http::sendFile(int cfd, char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        unix_error("open()");
    }
    char buf[1024] = "";
    int len = 0;
    while (1)
    {
        len = read(fd, buf, sizeof(buf));
        if (len < 0)
        {
            unix_error("read()");
        }
        else if (len == 0)
        {
            printf("请求文件内容为空!\n");
            break;
        }
        else
        {
            send(cfd, buf, len, 0);
            // printf("文件发送成功!\n");
        }
    }
    close(fd);
}

// 请求内容不存在, 发送404 not found
void Http::notFound(int cfd)
{
    char buf[1024] = "";
    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(cfd, buf, strlen(buf), 0);
}

// 客户端请求无效, 发送400 bad request
void Http::badRequest(int cfd)
{
    char buf[1024] = "";
    sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(cfd, buf, sizeof(buf), 0);
    sprintf(buf, "Maybe be your request is POST.\r\n");
    send(cfd, buf, sizeof(buf), 0);
}

// 关闭客户端套接字
void Http::closeConn(int cfd)
{
    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, ev);
    close(cfd);
}

// 获取发送文件类型
char *Http::getType(char *path)
{
    char *buf;
    // 从右向左查找字符'.',不存在则返回NULL
    buf = strrchr(path, '.');
    if (buf == NULL)
        return (char*)"text/html";
    printf("客户端请求的文件类型为%s\n", buf);
    if (strcmp(buf, ".html") == 0 || strcmp(buf, ".htm") == 0)
        return (char*)"text/html";
    else if (strcmp(buf, ".jpg") == 0 || strcmp(buf, ".jpeg") == 0)
        return (char*)"image/jpeg";
    else if (strcmp(buf, ".png") == 0)
        return (char*)"image/png";
    else if (strcmp(buf, ".gif") == 0)
        return (char*)"image/gif";
    else if (strcmp(buf, ".pdf") == 0)
        return (char*)"application/pdf";
    else if (strcmp(buf, ".doc") == 0)
        return (char*)"application/msword";
    else if (strcmp(buf, ".mp3") == 0)
        return (char*)"audio/mp3";
    return (char*)"text/plain";
}

// 拼接路径
char *Http::cat(char *path, const char *filename)
{
    memset(File, 0, sizeof(File));
    strcpy(File, path);
    strcat(File, filename);
    return File;
}

#endif