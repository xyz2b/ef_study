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

#ifndef _EFRAMEWORK_HEADER_
#define _EFRAMEWORK_HEADER_

#include "coroutine.h"
#include "util/list.h"
#include "poll.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#define FD_TYPE_LISTEN 1 // listen
#define FD_TYPE_RWC    2 // read (recv), write (send), connect

typedef struct _ef_routine ef_routine_t;
typedef struct _ef_runtime ef_runtime_t;
typedef struct _ef_queue_fd ef_queue_fd_t;
typedef struct _ef_poll_data ef_poll_data_t;
typedef struct _ef_listen_info ef_listen_info_t;

typedef long (*ef_routine_proc_t)(int fd, ef_routine_t *er);

struct _ef_poll_data {
    // socket类型（标识了socket能产生的事件类型，如服务端监听socket(FD_TYPE_LISTEN)会产生连接事件，客户端socket(FD_TYPE_RWC)会产生读写事件）
    int type;
    // socket
    int fd;
    // 负责此socket的协程
    ef_routine_t *routine_ptr;
    // 关联的runtime结构体
    ef_runtime_t *runtime_ptr;
    // 连接事件处理函数
    ef_routine_proc_t ef_proc;
};

// 客户端FD的队列
struct _ef_queue_fd {
    int fd;
    // 用于链接所有客户端FD的结构
    ef_list_entry_t list_entry;
};

struct _ef_listen_info {
    // IO多路复用器添加事件时传入的额外数据，用于关联多个数据结构
    ef_poll_data_t poll_data;
    // 新连接的事件处理函数
    ef_routine_proc_t ef_proc;
    // 用于链接到ef_runtime_t的监听链表的结构
    ef_list_entry_t list_entry;
    // 用于链接到ef_runtime_t的客户端FD列表的结构
    // 表示该监听socket所建立的所有客户端连接socket
    ef_list_entry_t fd_list;
};

struct _ef_runtime {
    // 多路复用器
    ef_poll_t *p;
    // 停止状态标志位
    int stopping;
    //
    int shrink_millisecs;
    //
    int count_per_shrink;
    // 协程池
    ef_coroutine_pool_t co_pool;
    // 监听链表，是个双向链表的结构，初始化时只有一个虚拟头节点，自己指向自己，有新的监听FD时，会将其封装成entry插入到这个链表末尾
    ef_list_entry_t listen_list;
    // 空闲的ef_queue_fd_t，一个ef_queue_fd_t表示一个客户端连接。缓存ef_queue_fd_t对象，因为客户端连接建立和断开比较频繁
    ef_list_entry_t free_fd_list;
};

struct _ef_routine {
    ef_coroutine_t co;
    ef_poll_data_t poll_data;
};

extern ef_runtime_t *ef_runtime;

#define ef_routine_current() ((ef_routine_t*)ef_coroutine_current(&ef_runtime->co_pool))

int ef_init(ef_runtime_t *rt, size_t stack_size, int limit_min, int limit_max, int shrink_millisecs, int count_per_shrink);
int ef_add_listen(ef_runtime_t *rt, int socket, ef_routine_proc_t ef_proc);
int ef_run_loop(ef_runtime_t *rt);

int ef_routine_close(ef_routine_t *er, int fd);
int ef_routine_connect(ef_routine_t *er, int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t ef_routine_read(ef_routine_t *er, int fd, void *buf, size_t count);
ssize_t ef_routine_write(ef_routine_t *er, int fd, const void *buf, size_t count);
ssize_t ef_routine_recv(ef_routine_t *er, int sockfd, void *buf, size_t len, int flags);
ssize_t ef_routine_send(ef_routine_t *er, int sockfd, const void *buf, size_t len, int flags);

#define ef_wrap_close(fd) \
    ef_routine_close(NULL, fd)

#define ef_wrap_connect(sockfd, addr, addrlen) \
    ef_routine_connect(NULL, sockfd, addr, addrlen)

#define ef_wrap_read(fd, buf, count) \
    ef_routine_read(NULL, fd, buf, count)

#define ef_wrap_write(fd, buf, count) \
    ef_routine_write(NULL, fd, buf, count)

#define ef_wrap_recv(sockfd, buf, len, flags) \
    ef_routine_recv(NULL, sockfd, buf, len, flags)

#define ef_wrap_send(sockfd, buf, len, flags) \
    ef_routine_send(NULL, sockfd, buf, len, flags)

#endif
