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

#ifndef NETTY_FSTACK_H_
#define NETTY_FSTACK_H_

#include <jni.h>
#include <limits.h>

struct runCtx {
    JNIEnv *env;
    jobject event_loop;

    int epfd;
    long address;
    int len;
};

JNIEXPORT jint netty_epoll_fstack_JNI_OnLoad(JNIEnv* env);

JNIEXPORT void netty_epoll_fstack_JNI_OnUnload(JNIEnv* env);

JNIEXPORT jint JNICALL Java_io_netty_channel_epoll_Native_fstackInit
  (JNIEnv *, jclass, jstring, jint, jstring);

JNIEXPORT void JNICALL Java_io_netty_channel_epoll_EpollEventLoop_fstackRun
  (JNIEnv *, jobject, jint, jlong, jint);

#endif
