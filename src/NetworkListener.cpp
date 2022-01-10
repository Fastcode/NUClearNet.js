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

#include <iostream>

namespace NUClear {
NetworkListener::NetworkListener(Napi::Env& env, NetworkBinding* binding)
: Napi::AsyncProgressWorker<char>(env), binding(binding) {
    std::vector<NUClear::fd_t> notifyfds = this->binding->net.listen_fds();

#ifdef _WIN32
    // Make an Event object for each of the notify fds
    for (auto& fd : notifyfds) {
        auto event = WSACreateEvent();
        if (event == WSA_INVALID_EVENT) {
            throw std::system_error(WSAGetLastError(),
                                    std::system_category(),
                                    "WSACreateEvent() for notify fd failed");
        }

        if (WSAEventSelect(fd, event, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
            throw std::system_error(WSAGetLastError(), std::system_category(), "WSAEventSelect() for notify fd failed");
        }

        this->fds.push_back(fd);
        this->events.push_back(event);
    }

    // Create an event to use for the notifier (used for getting out of WSAWaitForMultipleEvents())
    this->notifier = WSACreateEvent();
    if (this->notifier == WSA_INVALID_EVENT) {
        throw std::system_error(WSAGetLastError(), std::system_category(), "WSACreateEvent() for notifier failed");
    }

    this->events.push_back(notifier);
#else
    // Make a pollfd for each of the notify fds
    for (auto& fd : notifyfds) {
        this->fds.push_back(pollfd{fd, POLLIN, 0});
    }
#endif  // _WIN32
}

NetworkListener::~NetworkListener() {
#ifdef _WIN32
    for (auto& event : this->events) {
        if (!WSACloseEvent(event)) {
            std::cerr << "[NUClearNet.js NetworkListener] WSACloseEvent() failed, error code " << WSAGetLastError() << std::endl;
        }
    }
#endif
}

void NetworkListener::Execute(const Napi::AsyncProgressWorker<char>::ExecutionProgress& p) {
    bool run = true;

    // The run loop: runs until we get an FD close (setting run to false) or the network binding is destroyed
    while (run && !this->binding->destroyed) {
        bool data = false;

#ifdef _WIN32
        // Wait for events and check for shutdown
        auto event_index = WSAWaitForMultipleEvents(this->events.size(), this->events.data(), false, WSA_INFINITE, false);

        // Check if the return value is an event in our list
        if (event_index >= WSA_WAIT_EVENT_0 && event_index < WSA_WAIT_EVENT_0 + this->events.size()) {
            // Get the signalled event
            auto& event  = this->events[event_index - WSA_WAIT_EVENT_0];

            if (event == this->notifier) {
                // Reset the notifier signal
                if (!WSAResetEvent(event)) {
                    throw std::system_error(
                        WSAGetLastError(), std::system_category(), "WSAResetEvent() for notifier failed");
                }
            } else {
                // Get the corresponding fd for the event
                auto& fd = this->fds[event_index - WSA_WAIT_EVENT_0];

                // Enumumerate the socket events to work out which ones fired
                WSANETWORKEVENTS wsne;
                if (WSAEnumNetworkEvents(fd, event, &wsne) == SOCKET_ERROR) {
                    throw std::system_error(
                        WSAGetLastError(), std::system_category(), "WSAEnumNetworkEvents() failed");
                }

                // Exit the run loop if the fd was closed
                if ((wsne.lNetworkEvents & FD_CLOSE) != 0) {
                    run = false;
                }
                // Set the data flag if the fd has data to read
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

        // Notify the system something happened if we're running and have data to read.
        // Will trigger OnProgress() below to read the data.
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
