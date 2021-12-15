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
NetworkListener::NetworkListener(Napi::Env& env, NetworkBinding* binding)
: Napi::AsyncProgressWorker<char>(env), binding(binding) {
    std::vector<NUClear::fd_t> notifyfds = this->binding->net.listen_fds();

#ifdef _WIN32
    // Make an event for each file descriptor and link them up
    for (auto& fd : notifyfds) {
        auto event = WSACreateEvent(); // TODO: Check return status
        WSAEventSelect(fd, event, FD_READ | FD_CLOSE); // TODO: Check return status

        this->fds.push_back(fd);
        this->events.push_back(event);
    }

    // Create an event to use for the notifier (used for getting out of WSAWaitForMultipleEvents())
    this->notifier = WSACreateEvent(); // TODO: Check return status
    this->events.push_back(notifier);
#else
    // Make event
    for (auto& fd : notifyfds) {
        this->fds.push_back(pollfd{fd, POLLIN, 0});
    }
#endif  // _WIN32
}

NetworkListener::~NetworkListener() {
#ifdef _WIN32
    WSACloseEvent(this->notifier); // TODO: Check return status?

    for (auto& event : this->events) {
        WSACloseEvent(event); // TODO: Check return status?
    }
#endif
}

void NetworkListener::Execute(const Napi::AsyncProgressWorker<char>::ExecutionProgress& p) {
    bool run = true;
    while (run && !this->binding->destroyed) {
        bool data = false;

#ifdef _WIN32
        // Wait for events and check for shutdown
        auto eventIndex = WSAWaitForMultipleEvents(this->events.size(), this->events.data(), false, WSA_INFINITE, false);

        if (eventIndex >= WSA_WAIT_EVENT_0 && eventIndex < WSA_WAIT_EVENT_0 + this->events.size()) {
            auto& event  = this->events[eventIndex - WSA_WAIT_EVENT_0];

            if (event == this->notifier) {
                WSAResetEvent(event); // TODO: Check return status
            } else {
                auto& fd = this->fds[eventIndex - WSA_WAIT_EVENT_0];

                WSANETWORKEVENTS wsne;
                WSAEnumNetworkEvents(fd, event, &wsne); // TODO: Check return status

                if ((wsne.lNetworkEvents & FD_CLOSE) != 0) {
                    run = false;
                }
                else if ((wsne.lNetworkEvents & FD_READ) != 0) {
                    data = true;
                }
            }
        }
#else
        // Wait for events and check for shutdown
        poll(this->fds.data(), static_cast<nfds_t>(this->fds.size()), 500);

        // Check if the connections closed
        for (const auto& fd : this->fds) {
            if ((fd.revents & POLLNVAL) != 0) {
                run = false;
            }
            else if ((fd.revents & POLLIN) != 0) {
                data = true;
            }
        }
#endif  // _WIN32
        // Notify the system something happened if it's running
        if (run && data) {
            // Should really be `p.Signal()` here, but it has a bug at the moment
            // See https://github.com/nodejs/node-addon-api/issues/1081
            p.Send(nullptr, 0);
        }
    }
}

void NetworkListener::OnProgress(const char*, size_t) {
    // If we're here in OnProgress(), then there's data to process
    this->binding->net.process();
}

void NetworkListener::OnOK() {}

void NetworkListener::OnError(const Napi::Error& e) {}

}  // namespace NUClear
