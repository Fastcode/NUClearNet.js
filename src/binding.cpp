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

#include <nan.h>
#include "nuclear_bits/extension/network/NUClearNetwork.hpp"
#include "nuclear_bits/util/serialise/xxhash.h"

using NUClear::extension::network::NUClearNetwork;

class NetworkBinding : public Nan::ObjectWrap {
public:
    NetworkBinding() {}

    static void Send(const Nan::FunctionCallbackInfo<v8::Value>& info) {

        // info[0] == hash
        // info[1] == payload
        // info[2] == target
        // info[3] == reliable

        uint64_t hash = 0;
        std::vector<char> payload;
        std::string target = "";
        bool reliable      = false;

        switch (info.Length()) {
            case 4: {
                // Read reliablity information
                reliable = info[3]->BooleanValue();
            }

            // Intentional fallthrough
            case 3: {
                // Read target information
                // If we have a string here use it
                if (info[2]->IsString()) {
                    target = *Nan::Utf8String(info[2]);
                }
                // Otherwise, we accept null and undefined to mean everybody
                else if (!info[2]->IsUndefined() && !info[2]->IsNull()) {
                    Nan::ThrowError("Invalid target for the message");
                }
            }

            // Intentional fallthrough
            case 2: {
                Nan::TypedArrayContents<char> d(info[1]);
                if (*d == nullptr) {
                    Nan::ThrowError("Provided data to send was not a readable");
                }
                else {
                    // Put the data into the vector
                    payload.insert(payload.begin(), *d, *d + d.length());
                }
            }

            // Intentional fallthrough
            case 1:
                // If we have a string XXHash to get the hash
                if (info[0]->IsString()) {
                    std::string s = *Nan::Utf8String(info[2]);
                    hash          = XXH64(s.c_str(), s.size(), 0x4e55436c);
                }
                // Otherwise try to interpret it as a hash
                else {
                    Nan::TypedArrayContents<uint64_t> h(info[1]);
                    if (h.length() == 1) {
                        hash = (*h)[0];
                    }
                    else {
                        Nan::ThrowError("Invalid hash object");
                    }
                }
                break;

            default: { Nan::ThrowError("Wrong number of arguments for send"); } break;
        }

        // Perform the send
        try {
            NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
            bind->net.send(hash, payload, target, reliable);
        }
        catch (const std::exception& ex) {
            Nan::ThrowError(ex.what());
        }
    }

    static void On(const Nan::FunctionCallbackInfo<v8::Value>& info) {


        if (info[0]->IsString() && info[1]->IsFunction()) {

            std::string event = *Nan::Utf8String(info[0]);
            auto cb = std::make_shared<Nan::Callback>(info[1].As<v8::Function>());
            NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());

            if (event == "packet") {
                bind->net.set_packet_callback([cb=std::move(cb)] (const NUClearNetwork::NetworkTarget&, const uint64_t&, std::vector<char>&&) {
                    Nan::HandleScope scope;

                });
            }
            else if (event == "join" || event == "leave") {
                auto f = [cb=std::move(cb)] (const NUClearNetwork::NetworkTarget& t) {
                    Nan::HandleScope scope;

                    std::string name = t.name;
                    std::string address;
                    uint16_t port;

                    // Extract the IP address and port
                    char c[255];
                    std::memset(c, 0, sizeof(c));
                    switch (t.target.sock.sa_family) {
                        case AF_INET:
                            inet_ntop(t.target.sock.sa_family, &t.target.ipv4.sin_addr, c, sizeof(c));
                            port = ntohs(t.target.ipv4.sin_port);
                            break;

                        case AF_INET6:
                            inet_ntop(t.target.sock.sa_family, &t.target.ipv6.sin6_addr, c, sizeof(c));
                            port = ntohs(t.target.ipv6.sin6_port);
                            break;
                    }

                    v8::Local<v8::String> n = Nan::New<v8::String>(name).ToLocalChecked();
                    v8::Local<v8::String> a = Nan::New<v8::String>(address).ToLocalChecked();
                    v8::Local<v8::Integer> p = Nan::New<v8::Integer>(port);

                    v8::Local<v8::Value> v[3] = {
                        n.As<v8::Value>(),
                        a.As<v8::Value>(),
                        p.As<v8::Value>()
                    };

                    cb->Call(3, v);
                };

                if (event == "join") {
                    bind->net.set_join_callback(std::move(f));
                }
                else {
                    bind->net.set_leave_callback(std::move(f));
                }
            }
            else if (event == "wait") {
                bind->net.set_next_event_callback([cb=std::move(cb)] (std::chrono::steady_clock::time_point t) {
                    Nan::HandleScope scope;

                    using namespace std::chrono;
                    int ms = duration_cast<milliseconds>(t - steady_clock::now()).count();
                    ms++; // Add 1 to account for any funky rounding

                    v8::Local<v8::Integer> v = Nan::New<v8::Integer>(ms);
                    v8::Local<v8::Value> argv = v.As<v8::Value>();
                    cb->Call(1, &argv);
                });
            }

        }
        else {
        }

        // info[0] should be a string of type 'packet', 'join', 'leave', 'wait'

        // void set_packet_callback(std::function<void(const NetworkTarget&, const uint64_t&, std::vector<char>&&)> f);
        // void set_join_callback(std::function<void(const NetworkTarget&)> f);
        // void set_leave_callback(std::function<void(const NetworkTarget&)> f);
        // void set_next_event_callback(std::function<void(std::chrono::steady_clock::time_point)> f);
    }

    static void Reset(const Nan::FunctionCallbackInfo<v8::Value>& info) {

        std::string name     = "";
        std::string group    = "239.226.152.162";
        uint16_t port        = 7447;
        uint16_t network_mtu = 1500;

        switch (info.Length()) {
            case 4: {
                // MTU number
                if (info[3]->IsNumber()) {
                    network_mtu = info[3]->IntegerValue();
                }
                else {
                    Nan::ThrowError("MTU is not a number");
                }
            }

            case 3: {
                // Port number
                if (info[2]->IsNumber()) {
                    port = info[2]->IntegerValue();
                }
                else {
                    Nan::ThrowError("Port is not a number");
                }
            }

            case 2: {
                // Multicast Group
                if (info[1]->IsString()) {
                    group = *Nan::Utf8String(info[1]);
                }
                else {
                    Nan::ThrowError("Invalid multicast group");
                }
            }

            case 1: {
                // Name Group
                if (info[0]->IsString()) {
                    name = *Nan::Utf8String(info[0]);
                }
                else {
                    Nan::ThrowError("Invalid name group");
                }
            } break;

            default: { Nan::ThrowError("Wrong number of arguments"); } break;
        }

        // Perform the reset
        try {
            NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
            bind->net.reset(name, group, port, network_mtu);
        }
        catch (const std::exception& ex) {
            Nan::ThrowError(ex.what());
        }
    }

    static void Process(const Nan::FunctionCallbackInfo<v8::Value>& info) {
        // Perform the process function
        try {
            NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
            bind->net.process();
        }
        catch (const std::exception& ex) {
            Nan::ThrowError(ex.what());
        }
    }

    static void Shutdown(const Nan::FunctionCallbackInfo<v8::Value>& info) {
        // Perform the shutdown function
        try {
            NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
            bind->net.shutdown();
        }
        catch (const std::exception& ex) {
            Nan::ThrowError(ex.what());
        }
    }


    /**
     * TODO std::vector<int> listen_fds();`
     */

    static void Init(v8::Local<v8::Object> exports) {
        Nan::HandleScope scope;

        // Prepare constructor template
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
        tpl->SetClassName(Nan::New("NetworkBinding").ToLocalChecked());
        tpl->InstanceTemplate()->SetInternalFieldCount(1);

        // Prototype
        Nan::SetPrototypeMethod(tpl, "send", Send);
        Nan::SetPrototypeMethod(tpl, "on", On);
        Nan::SetPrototypeMethod(tpl, "reset", Reset);
        Nan::SetPrototypeMethod(tpl, "process", Process);
        Nan::SetPrototypeMethod(tpl, "shutdown", Shutdown);

        constructor.Reset(tpl->GetFunction());
        exports->Set(Nan::New("NetworkBinding").ToLocalChecked(), tpl->GetFunction());
    }

    static void New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
        // Invoked as constructor: `new MyObject(...)`
        if (info.IsConstructCall()) {
            NetworkBinding* obj = new NetworkBinding();
            obj->Wrap(info.This());
            info.GetReturnValue().Set(info.This());
        }
        // Invoked as function: `MyObject(...)` convert to construct call
        else {
            const int argc                  = 0;
            v8::Local<v8::Value> argv[argc] = {};
            v8::Local<v8::Function> cons    = Nan::New<v8::Function>(constructor);
            info.GetReturnValue().Set(Nan::NewInstance(cons, argc, argv).ToLocalChecked());
        }
    }
    static Nan::Persistent<v8::Function> constructor;

private:
    NUClearNetwork net;
};

Nan::Persistent<v8::Function> NetworkBinding::constructor;

void Init(v8::Local<v8::Object> exports) {
    NetworkBinding::Init(exports);
}

NODE_MODULE(nuclearnet, Init)
