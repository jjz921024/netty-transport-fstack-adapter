/*
 * Copyright 2016 The Netty Project
 *
 * The Netty Project licenses this file to you under the Apache License,
 * version 2.0 (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at:
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#define __USE_GNU
#include <pthread.h>

#include "ff_config.h"
#include "ff_api.h"
#include "ff_epoll.h"
#include "netty_epoll_fstack.h"

static jmethodID runAllTasksMethodId = NULL;
static jmethodID processReadyMethodId = NULL;

static jfieldID ioRatioFieldId = NULL;
static jfieldID eventsFieldId = NULL;

static inline void timespec_sub(struct timespec *t1, const struct timespec *t2) {
    t1->tv_sec -= t2->tv_sec;
    t1->tv_nsec -= t2->tv_nsec;
    if (t1->tv_nsec < 0) {
        t1->tv_sec--;
        t1->tv_nsec += 1000000000;
    }
}

// copy from netty_epoll_native.c
static inline int netty_epoll_wait(JNIEnv* env, jint efd, struct epoll_event *ev, jint len, jint timeout) {
    int rc;
    if (timeout <= 0) {
        while ((rc = ff_epoll_wait(efd, ev, len, timeout)) < 0) {
            if (errno != EINTR) {
                return -errno;
            }
        }
    } else {
        // timeout大于0时
        struct timespec ts;
        long deadline, now;
        if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
        // 计算deadline
        deadline = ts.tv_sec * 1000 + ts.tv_nsec / 1000 + timeout;
        while ((rc = ff_epoll_wait(efd, ev, len, timeout)) < 0) {
            if (errno != EINTR) return -errno;
            if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return -1;
            now = ts.tv_sec * 1000 + ts.tv_nsec / 1000;
            if (now >= deadline) {
                return 0;
            }
            timeout = deadline - now;
        }
    }
    return rc;
}

// 在c中实现EpollEventLoop的run方法的逻辑
// TODO：如何响应退出信号
int loop(void *arg) {
    struct runCtx *ctx = (struct runCtx *)(arg);
    JNIEnv *env = ctx->env;
    struct epoll_event *ev = (struct epoll_event *)(intptr_t)(ctx->address);

    int strategy = 0;
    if ((strategy = netty_epoll_wait(env, ctx->epfd, ev, ctx->len, 0)) < 0) return -1;

    long ioTime = 0;
    if (strategy > 0) {
        struct timespec ioStartTime, ioEndTime;
        if (clock_gettime(CLOCK_MONOTONIC, &ioStartTime) == -1) return -1;

        // 处理已就绪的事件
        jobject events = (*env)->GetObjectField(env, ctx->event_loop, eventsFieldId);
        (*env)->CallBooleanMethod(env, ctx->event_loop, processReadyMethodId, events, strategy);

        if (clock_gettime(CLOCK_MONOTONIC, &ioEndTime) == -1) return -1;

        jint ioRatio = (*env)->GetIntField(env, ctx->event_loop, ioRatioFieldId);
        timespec_sub(&ioEndTime, &ioStartTime);
        ioTime = ((long) ioEndTime.tv_sec * 1000000000 + (long) ioEndTime.tv_nsec) * (100 - ioRatio) / ioRatio;
    }

    // 处理非IO任务
    (*env)->CallBooleanMethod(env, ctx->event_loop, runAllTasksMethodId, ioTime);
}

JNIEXPORT jint JNICALL Java_io_netty_channel_epoll_Native_fstackInit(
                                             JNIEnv *env, jclass clazz,
                                             jstring conf_path, jint id, jstring type) {
    const char *path = (*env)->GetStringUTFChars(env, conf_path, NULL);
    if (path == NULL) goto error;

    char id_buf[30];
    int len = sprintf(id_buf, "--proc-id=%d", id);
    id_buf[len] = '\0';

    const char *t = (*env)->GetStringUTFChars(env, type, NULL);
    if (t == NULL) goto error;
    char type_buf[50];
    len = sprintf(type_buf, "--proc-type=%s", t);
    type_buf[len] = '\0';

    char *ff_argv[] = { "", "--conf", (char *)path, id_buf, type_buf };
    if (ff_init(5, ff_argv) < 0) {
        printf("fstack init failed\n");
        goto error;
    }

    printf("fstack init success\n");
    fflush(stdout);
    (*env)->ReleaseStringUTFChars(env, conf_path, path);
    (*env)->ReleaseStringUTFChars(env, type, t);
    return 0;

error:
    if (path != NULL) (*env)->ReleaseStringUTFChars(env, conf_path, path);
    if (t != NULL) (*env)->ReleaseStringUTFChars(env, type, t);
    return -1;
}

// 必须在EpollEventLoop类中调用, 方便能将event loop实例传入
JNIEXPORT void JNICALL Java_io_netty_channel_epoll_EpollEventLoop_fstackRun(
                                JNIEnv *env, jobject obj,
                                jint epfd, jlong address, jint len) {
    struct runCtx ctx = {
        .env = env,
        .event_loop = obj,
        .epfd = epfd,
        .address = address,
        .len = len,
    };
    ff_run(loop, &ctx);
}

JNIEXPORT jint netty_epoll_fstack_JNI_OnLoad(JNIEnv* env) {
    printf("netty_epoll_fstack lib load\n");
    /* JNIEnv* env = NULL;
    if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {}
    return JNI_VERSION_1_6 */

    // FindClass获取的epollEventLoopClass为本地对象, 在方法返回时自动释放
    jclass epollEventLoopClass = (*env)->FindClass(env, "io/netty/channel/epoll/EpollEventLoop");
    if (epollEventLoopClass == NULL) {
        printf("cannot get class SingleThreadEventLoop\n");
        return -1;
    }

    processReadyMethodId = (*env)->GetMethodID(env, epollEventLoopClass, "processReady",
                                   "(Lio/netty/channel/epoll/EpollEventArray;I)Z");
    if (processReadyMethodId == NULL) {
        printf("cannot get method processReady\n");
        return -1;
    }

    runAllTasksMethodId = (*env)->GetMethodID(env, epollEventLoopClass, "runAllTasks", "(J)Z");
    if (runAllTasksMethodId == NULL) {
        printf("cannot get method runAllTasks\n");
        return -1;
    }

    ioRatioFieldId = (*env)->GetFieldID(env, epollEventLoopClass, "ioRatio", "I");
    if (ioRatioFieldId == NULL) {
        printf("cannot get field ioRatio\n");
        return -1;
    }

    eventsFieldId = (*env)->GetFieldID(env, epollEventLoopClass, "events",
                                        "Lio/netty/channel/epoll/EpollEventArray;");
    if (eventsFieldId == NULL) {
        printf("cannot get field events\n");
        return -1;
    }
    return 0;
}

JNIEXPORT void netty_epoll_fstack_JNI_OnUnload(JNIEnv* env) {
    printf("netty_epoll_fstack lib unload\n");
    runAllTasksMethodId = NULL;
    processReadyMethodId = NULL;
    ioRatioFieldId = NULL;
    eventsFieldId = NULL;
}