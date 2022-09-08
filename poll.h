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

#ifndef _POLL_HEADER_
#define _POLL_HEADER_

typedef struct _ef_event ef_event_t;
typedef struct _ef_poll ef_poll_t;

typedef ef_poll_t *(*create_func_t)(int cap);

typedef int (*associate_func_t)(ef_poll_t *p, int fd, int events, void *ptr, int fired);
typedef int (*dissociate_func_t)(ef_poll_t *p, int fd, int fired, int onclose);
typedef int (*unset_func_t)(ef_poll_t *p, int fd, int events);
typedef int (*wait_func_t)(ef_poll_t *p, ef_event_t *evts, int count, int millisecs);
typedef int (*free_func_t)(ef_poll_t *p);

struct _ef_event {
    int events;
    void *ptr;
};

// 封装了多路复用器一些操作的接口
struct _ef_poll {
    // 注册某个FD的感兴趣事件到IO多路复用器
    associate_func_t associate;
    // 解注册某个FD的感兴趣事件到IO多路复用器
    dissociate_func_t dissociate;
    // 暂时无用
    unset_func_t unset;
    // 获取就绪的事件
    wait_func_t wait;
    // 释放多路复用器
    free_func_t free;
};

// 创建多路复用器
extern create_func_t ef_create_poll;

/*
 * the macros are equal in poll and epoll
 */
#define EF_POLLIN  0x001
#define EF_POLLOUT 0x004
#define EF_POLLERR 0x008
#define EF_POLLHUP 0x010

#endif