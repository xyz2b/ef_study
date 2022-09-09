// Copyright (c) 2018-2020 The EFramework Project
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <stdio.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "framework.h"

// 协程事件循环主结构体
ef_runtime_t efr = {0};

#define BUFFER_SIZE 8192

// for performance test
// forward port 8080 <=> 80, HTTP GET only
// let a http server run at localhost:80
// use HTTP/1.0 or set http header 'Connection: Close'
long forward_proc(int fd, ef_routine_t *er)
{
    char buffer[BUFFER_SIZE];
    ssize_t r = ef_routine_read(er, fd, buffer, BUFFER_SIZE);
    if(r <= 0)
    {
        return r;
    }
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(80);
    int ret = ef_routine_connect(er, sockfd, (const struct sockaddr *)&addr_in, sizeof(addr_in));
    if(ret < 0)
    {
        return ret;
    }
    ssize_t w = ef_routine_write(er, sockfd, buffer, r);
    if(w < 0)
    {
        goto exit_proc;
    }
    while(1)
    {
        r = ef_routine_read(er, sockfd, buffer, BUFFER_SIZE);
        if(r <= 0)
        {
            break;
        }
        ssize_t wrt = 0;
        while(wrt < r)
        {
            w = ef_routine_write(er, fd, &buffer[wrt], r - wrt);
            if(w < 0)
            {
                goto exit_proc;
            }
            wrt += w;
        }
    }
    exit_proc:
    ef_routine_close(er, sockfd);
    return ret;
}

long greeting_proc(int fd, ef_routine_t *er)
{
    char resp_ok[] = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 26\r\nContent-Type: text/plain; charset=utf-8\r\n\r\nWelcome to the EFramework!";
    char buffer[BUFFER_SIZE];
    ssize_t r = ef_routine_read(er, fd, buffer, BUFFER_SIZE);
    if(r <= 0)
    {
        return r;
    }
    r = sizeof(resp_ok) - 1;
    ssize_t wrt = 0;
    while(wrt < r)
    {
        ssize_t w = ef_routine_write(er, fd, &resp_ok[wrt], r - wrt);
        if(w < 0)
        {
            return w;
        }
        wrt += w;
    }
    return 0;
}

void signal_handler(int num)
{
    // 将协程事件循环的停止状态标志位置为1，标识为退出事件循环
    efr.stopping = 1;
}

int main(int argc, char *argv[])
{
    // 1. 初始化框架
    // 协程池初始化，需要指定协程池规模，协程栈大小
    // IO多路复用初始化
    if (ef_init(&efr, 64 * 1024, 256, 512, 1000 * 60, 16) < 0) {
        return -1;
    }

    // 注册退出信号处理函数
    struct sigaction sa = {0};
    sa.sa_handler = signal_handler;
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // 2. 创建监听socket
    // 监听8080端口
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        return -1;
    }
    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(8080);
    int retval = bind(sockfd, (const struct sockaddr *)&addr_in, sizeof(addr_in));
    if(retval < 0)
    {
        return -1;
    }
    listen(sockfd, 512);
    // 将新监听的socket，添加到ef_runtime_t的监听链表开头
    // 框架支持多个监听socket分别监听不同端口，所以先放入链表，框架运行起来后会一并处理
    // 需要指定业务处理入口，此处为forward_proc
    // 新建立的连接会交给一个协程，forward_proc便是这些协程的执行入口
    ef_add_listen(&efr, sockfd, forward_proc);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        return -1;
    }
    addr_in.sin_port = htons(8081);
    retval = bind(sockfd, (const struct sockaddr *)&addr_in, sizeof(addr_in));
    if(retval < 0)
    {
        return -1;
    }
    listen(sockfd, 512);
    ef_add_listen(&efr, sockfd, greeting_proc);

    // 启动协程事件循环
    return ef_run_loop(&efr);
}
