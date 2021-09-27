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

#include "nuclear/src/extension/network/NUClearNetwork.hpp"
#include <napi.h>

namespace NUClear {

class NetworkBinding : public Napi::ObjectWrap<NetworkBinding> {
public:
    NetworkBinding(const Napi::CallbackInfo& info);

    Napi::Value Hash(const Napi::CallbackInfo& info);
    void Send(const Napi::CallbackInfo& info);
    void On(const Napi::CallbackInfo& info);
    void Reset(const Napi::CallbackInfo& info);
    void Process(const Napi::CallbackInfo& info);
    void Shutdown(const Napi::CallbackInfo& info);
    
    extension::network::NUClearNetwork net;

    static void Init(Napi::Env env, Napi::Object exports);

    // void New(const Napi::CallbackInfo& info);
    // Napi::FunctionReference constructor;
};

}  // namespace NUClear

#endif  // NETWORKBINDING_H
