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

#include "NetworkListener.hpp"

namespace NUClear {
NetworkListener::NetworkListener(Nan::Callback* callback, NetworkBinding* binding, std::vector<NUClear::fd_t> notifyfds)
    : Nan::AsyncProgressWorker(callback), binding(binding) {
#ifdef _WIN32
    // Make event and link it up
    for (auto& fd : notifyfds) {
        auto event = WSACreateEvent();
        WSAEventSelect(fd, event, FD_READ | FD_CLOSE);

        fds.push_back(fd);
        events.push_back(event);
    }
#else
    // Make event
    for (auto& fd : notifyfds) {
        fds.push_back(pollfd{fd, POLLIN, 0});
    }
#endif  // _WIN32
}

void NetworkListener::Execute(const ExecutionProgress& p) {
    bool run = true;
    while (run) {
#ifdef _WIN32
        // Wait for events and check for shutdown
        auto event = WSAWaitForMultipleEvents(events.size(), events.data(), false, WSA_INFINITE, false);

        if (event >= WSA_WAIT_EVENT_0 && event < WSA_WAIT_EVENT_0 + events.size()) {
            auto& e = events[event - WSA_WAIT_EVENT_0];
            auto& fd = fds[event - WSA_WAIT_EVENT_0];

            WSANETWORKEVENTS wsne;
            WSAEnumNetworkEvents(fd, e, &wsne);

            if((wsne.lNetworkEvents & FD_CLOSE) != 0) {
                run = false;
            }
        }
#else
        // Wait for events and check for shutdown
        poll(fds.data(), static_cast<nfds_t>(fds.size()), -1);

        // Check if the connections closed
        for (const auto& fd : fds) {
            if ((fd.revents & POLLNVAL) != 0) {
                run = false;
            }
        }
#endif  // _WIN32
        // Notify the system something happened if it's running
        if (run) {
            p.Signal();
        }
    }
}

void NetworkListener::HandleProgressCallback(const char*, size_t) {
    Nan::HandleScope scope;

    // Call what should be process
    binding->net.process();
}
}
