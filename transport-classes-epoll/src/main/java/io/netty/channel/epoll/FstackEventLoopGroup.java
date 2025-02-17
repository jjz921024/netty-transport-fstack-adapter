/*
 * Copyright 2014 The Netty Project
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
package io.netty.channel.epoll;

import io.netty.channel.DefaultSelectStrategyFactory;
import io.netty.channel.EventLoop;
import io.netty.channel.MultithreadEventLoopGroup;
import io.netty.util.concurrent.RejectedExecutionHandlers;

import java.util.concurrent.Executor;
import java.util.concurrent.ThreadFactory;

public class FstackEventLoopGroup extends MultithreadEventLoopGroup {

    private int index = 0;

    // 创建fstack线程, primary线程只能有一个
    public FstackEventLoopGroup(int nThreads, ThreadFactory threadFactory,
                                String confPath, boolean isPrimary, int[] procIds) {
        super(nThreads, threadFactory, confPath, isPrimary, procIds);
        if (procIds.length == 0) {
            throw new RuntimeException("proc id list can not be null");
        }
    }

    @Override
    protected EventLoop newChild(Executor executor, Object... args) throws Exception {
        String confPath = (String) args[0];
        boolean isPrimary = (Boolean) args[1];
        int[] procIds = (int[]) args[2];
        int procId = procIds[index % procIds.length];
        EpollEventLoop eventLoop = new EpollEventLoop(this, executor,
                confPath, procId, (isPrimary && index == 0) ? "primary" : "secondary");
        index++;
        return eventLoop;
    }
}
