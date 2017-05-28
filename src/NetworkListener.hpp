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

#ifndef NETWORKLISTENER_H
#define NETWORKLISTENER_H

#include "NetworkBinding.hpp"
#include <nan.h>

namespace NUClear {

class NetworkListener : public Nan::AsyncProgressWorker {
public:
    NetworkListener(NetworkBinding* binding);
    void Execute(const ExecutionProgress& p);
    void HandleProgressCallback(const char*, size_t);
    void HandleOKCallback();
    void HandleErrorCallback();

    NetworkBinding* binding;

#ifdef _WIN32
    std::vector<WSAEVENT> events;
    std::vector<SOCKET> fds;
#else
    std::vector<pollfd> fds;
#endif  // _WIN32
};

}  // namespace NUClear
#endif  // NETWORKLISTENER_H
