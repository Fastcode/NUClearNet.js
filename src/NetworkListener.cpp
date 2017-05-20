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

NetworkListener::NetworkListener(Nan::Callback* callback, NetworkBinding* binding, std::vector<NUClear::fd_t> notifyfds)
    : Nan::AsyncProgressWorker(callback), binding(binding) {
#ifdef _WIN32
    // Make an event that can shut it down
    notifier = WSACreateEvent();

    // Make event and link it up
    for (auto& fd : notifyfds) {
        auto event = WSACreateEvent();
        WSAEventSelect(fd, event, FD_READ);
        fds.push_back(event);
    }
#else
    // Make notifier so it can be shut down
    int vals[2];
    pipe(vals);
    notify_recv = vals[0];
    notify_send = vals[1];
    fds.push_back(pollfd{notify_recv, POLLIN, 0});

    // Make event
    for (auto& fd : notifyfds) {
        fds.push_back(pollfd{fd, POLLIN, 0});
    }
#endif  // _WIN32
}

NetworkListener::~NetworkListener(){};

void NetworkListener::Execute(const ExecutionProgress& p) {

    while (true) {
#ifdef _WIN32
        // Wait for events and check for shutdown
        auto event = WSAWaitForMultipleEvents(fds.size(), fds.data(), false, WSA_INFINITE, false);
        if (event == WSA_WAIT_EVENT_0) break;
#else
        // Wait for events and check for shutdown
        poll(fds.data(), static_cast<nfds_t>(fds.size()), -1);
        if (fds[0].revents) break;
#endif  // _WIN32
        // Notify the system something happened
        p.Signal();
    }
}

void NetworkListener::HandleProgressCallback(const char*, size_t) {
    Nan::HandleScope scope;

    // Call what should be process
    binding->net.process();
}

void NetworkListener::Destroy() {
#ifdef _WIN32
    // Activate the notifier
    WSASetEvent(notifier);
#else
    // Activate the notifier
    char val = 0;
    write(notify_send, &val, 1);
#endif  // _WIN32
}
