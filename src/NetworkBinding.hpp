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

#include <nan.h>
#include "nuclear_bits/extension/network/NUClearNetwork.hpp"

using NUClear::extension::network::NUClearNetwork;

class NetworkBinding : public Nan::ObjectWrap {
public:
    NetworkBinding();
    static void Hash(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Send(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void On(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Reset(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Process(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Shutdown(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static void Init(v8::Local<v8::Object> exports, v8::Local<v8::Object> module);
    static void New(const Nan::FunctionCallbackInfo<v8::Value>& info);
    static Nan::Persistent<v8::Function> constructor;

    NUClearNetwork net;
};

#endif  // NETWORKBINDING_H
