/*
 * Copyright (C) 2013-2016 Trent Houliston <trent@houliston.me>, Jake Woods <jake.f.woods@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef NETWORKBINDING_H
#define NETWORKBINDING_H

#include <atomic>

#include <napi.h>

#include "nuclear/src/nuclearnet/NUClearNet.hpp"

namespace NUClear {

class NetworkListener;

class NetworkBinding : public Napi::ObjectWrap<NetworkBinding> {
public:
    NetworkBinding(const Napi::CallbackInfo& info);

    Napi::Value Hash(const Napi::CallbackInfo& info);
    void Send(const Napi::CallbackInfo& info);
    void OnPacket(const Napi::CallbackInfo& info);
    void OnJoin(const Napi::CallbackInfo& info);
    void OnLeave(const Napi::CallbackInfo& info);
    void OnWait(const Napi::CallbackInfo& info);
    void Reset(const Napi::CallbackInfo& info);
    void Process(const Napi::CallbackInfo& info);
    void Shutdown(const Napi::CallbackInfo& info);
    void Destroy(const Napi::CallbackInfo& info);
    void AddSubscription(const Napi::CallbackInfo& info);
    void SetSubscriptions(const Napi::CallbackInfo& info);
    void SetLogLevel(const Napi::CallbackInfo& info);

    void stop_listener();
    void start_listener(Napi::Env env);
    void request_listener_restart();

    network::NUClearNet net;
    bool destroyed = false;
    std::atomic<uint32_t> listener_generation{0};
    Napi::ThreadSafeFunction on_packet;
    Napi::ThreadSafeFunction on_join;
    Napi::ThreadSafeFunction on_leave;
    Napi::ThreadSafeFunction on_wait;
    Napi::ThreadSafeFunction listener_restart;

#ifdef _WIN32
    WSAEVENT listener_notifier = WSA_INVALID_EVENT;
#endif

    static void Init(Napi::Env env, Napi::Object exports);
};

}  // namespace NUClear

#endif  // NETWORKBINDING_H
