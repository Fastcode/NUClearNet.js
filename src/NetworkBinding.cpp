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

#include "NetworkBinding.hpp"
#include "NetworkListener.hpp"
#include "nuclear/src/util/serialise/xxhash.h"

namespace NUClear {

using extension::network::NUClearNetwork;

NetworkBinding::NetworkBinding(const Napi::CallbackInfo& info): Napi::ObjectWrap<NetworkBinding>(info) {}

void NetworkBinding::Hash(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    if (info[0]->IsString()) {
        // Calculate hash
        std::string s = *Nan::Utf8String(info[0]);
        uint64_t hash = XXH64(s.c_str(), s.size(), 0x4e55436c);

        // Return hash
        info.GetReturnValue().Set(
            Nan::CopyBuffer(reinterpret_cast<const char*>(&hash), sizeof(uint64_t)).ToLocalChecked().As<v8::Value>());
    }
    // Otherwise, we accept null and undefined to mean everybody
    else {
        Nan::ThrowError("Can only hash strings");
    }
}

void NetworkBinding::Send(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    // info[0] == hash
    // info[1] == payload
    // info[2] == target
    // info[3] == reliable

    uint64_t hash = 0;
    std::vector<char> payload;
    std::string target = "";
    bool reliable      = false;

    // Read reliablity information
    reliable = Nan::To<bool>(info[3]).ToChecked();

    // Read target information
    // If we have a string here use it
    if (info[2]->IsString()) {
        target = *Nan::Utf8String(info[2]);
    }
    // Otherwise, we accept null and undefined to mean everybody
    else if (!info[2]->IsUndefined() && !info[2]->IsNull()) {
        Nan::ThrowError("Invalid target for the message");
    }

    // Read the data information
    Nan::TypedArrayContents<char> d(info[1]);
    if (*d == nullptr) {
        Nan::ThrowError("Provided data to send was not a readable");
    }
    else {
        // Put the data into the vector
        payload.insert(payload.begin(), *d, *d + d.length());
    }

    // If we have a string XXHash to get the hash
    if (info[0]->IsString()) {
        std::string s = *Nan::Utf8String(info[0]);
        hash          = XXH64(s.c_str(), s.size(), 0x4e55436c);
    }
    // Otherwise try to interpret it as a hash
    else {
        Nan::TypedArrayContents<uint8_t> h(info[0]);
        if (h.length() == 8) {
            std::memcpy(&hash, *h, 8);
        }
        else {
            Nan::ThrowError("Invalid hash object");
        }
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

void NetworkBinding::On(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    if (info[0]->IsString() && info[1]->IsFunction()) {

        std::string event    = *Nan::Utf8String(info[0]);
        auto cb              = std::make_shared<Nan::Callback>(info[1].As<v8::Function>());
        NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());

        if (event == "packet") {
            bind->net.set_packet_callback([cb = std::move(cb)](
                const NUClearNetwork::NetworkTarget& t, const uint64_t& hash, const bool& reliable, std::vector<char>&& payload) {
                Nan::HandleScope scope;

                std::string name = t.name;
                std::string address;
                uint16_t port;

                // Extract the IP address and port
                char c[255];
                std::memset(c, 0, sizeof(c));
                switch (t.target.sock.sa_family) {
                    case AF_INET:
                        inet_ntop(t.target.sock.sa_family, const_cast<in_addr*>(&t.target.ipv4.sin_addr), c, sizeof(c));
                        port = ntohs(t.target.ipv4.sin_port);
                        break;

                    case AF_INET6:
                        inet_ntop(
                            t.target.sock.sa_family, const_cast<in6_addr*>(&t.target.ipv6.sin6_addr), c, sizeof(c));
                        port = ntohs(t.target.ipv6.sin6_port);
                        break;
                }
                address = c;

                v8::Local<v8::Value> argv[6] = {
                    Nan::New<v8::String>(name).ToLocalChecked().As<v8::Value>(),
                    Nan::New<v8::String>(address).ToLocalChecked().As<v8::Value>(),
                    Nan::New<v8::Integer>(port).As<v8::Value>(),
                    Nan::New<v8::Boolean>(reliable).As<v8::Value>(),
                    Nan::CopyBuffer(reinterpret_cast<const char*>(&hash), sizeof(uint64_t))
                        .ToLocalChecked()
                        .As<v8::Value>(),
                    Nan::CopyBuffer(payload.data(), payload.size()).ToLocalChecked().As<v8::Value>()};

                Nan::Call(*cb, 6, argv);
            });
        }
        else if (event == "join" || event == "leave") {
            auto f = [cb = std::move(cb)](const NUClearNetwork::NetworkTarget& t) {
                Nan::HandleScope scope;

                std::string name = t.name;
                std::string address;
                uint16_t port = 0;

                // Extract the IP address and port
                char c[255];
                std::memset(c, 0, sizeof(c));
                switch (t.target.sock.sa_family) {
                    case AF_INET:
                        inet_ntop(t.target.sock.sa_family, const_cast<in_addr*>(&t.target.ipv4.sin_addr), c, sizeof(c));
                        port = ntohs(t.target.ipv4.sin_port);
                        break;

                    case AF_INET6:
                        inet_ntop(
                            t.target.sock.sa_family, const_cast<in6_addr*>(&t.target.ipv6.sin6_addr), c, sizeof(c));
                        port = ntohs(t.target.ipv6.sin6_port);
                        break;

                    default: Nan::ThrowError("The system has a corrupted network peer record.");
                }
                address = c;

                v8::Local<v8::Value> argv[3] = {Nan::New<v8::String>(name).ToLocalChecked().As<v8::Value>(),
                                                Nan::New<v8::String>(address).ToLocalChecked().As<v8::Value>(),
                                                Nan::New<v8::Integer>(port).As<v8::Value>()};

                Nan::Call(*cb, 3, argv);
            };

            if (event == "join") {
                bind->net.set_join_callback(std::move(f));
            }
            else {
                bind->net.set_leave_callback(std::move(f));
            }
        }
        else if (event == "wait") {
            bind->net.set_next_event_callback([cb = std::move(cb)](std::chrono::steady_clock::time_point t) {
                Nan::HandleScope scope;

                using namespace std::chrono;
                int ms = duration_cast<std::chrono::duration<int, std::milli>>(t - steady_clock::now()).count();
                ms++;  // Add 1 to account for any funky rounding

                v8::Local<v8::Integer> v  = Nan::New<v8::Integer>(ms);
                v8::Local<v8::Value> argv = v.As<v8::Value>();
                Nan::Call(*cb, 1, &argv);
            });
        }
    }
    else {
        Nan::ThrowError("on expects a string event name and a function");
    }
}

void NetworkBinding::Reset(const Nan::FunctionCallbackInfo<v8::Value>& info) {

    // info[0] = name
    // info[1] = mutlicast group
    // info[2] = port
    // info[3] = mtu

    std::string name     = "";
    std::string group    = "239.226.152.162";
    uint32_t port        = Nan::To<uint32_t>(info[2]).FromMaybe(7447);
    uint32_t network_mtu = Nan::To<uint32_t>(info[3]).FromMaybe(1500);

    // Multicast Group
    if (info[1]->IsString()) {
        group = *Nan::Utf8String(info[1]);
    }
    else {
        Nan::ThrowError("Multicast group must be a string");
    }

    // Name Group
    if (info[0]->IsString()) {
        name = *Nan::Utf8String(info[0]);
    }
    else {
        Nan::ThrowError("Name must be a string");
    }

    // Perform the reset
    try {
        NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
        bind->net.reset(name, group, port, network_mtu);

        Nan::AsyncQueueWorker(new NetworkListener(bind));
    }
    catch (const std::exception& ex) {
        Nan::ThrowError(ex.what());
    }
}

void NetworkBinding::Process(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    // Perform the process function
    try {
        NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
        bind->net.process();
    }
    catch (const std::exception& ex) {
        Nan::ThrowError(ex.what());
    }
}

void NetworkBinding::Shutdown(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    // Perform the shutdown function
    try {
        NetworkBinding* bind = ObjectWrap::Unwrap<NetworkBinding>(info.Holder());
        bind->net.shutdown();
    }
    catch (const std::exception& ex) {
        Nan::ThrowError(ex.what());
    }
}

void NetworkBinding::Init(Napi::Env env, Napi::Object exports) {
    // Napi::HandleScope scope(env);

    Napi::Function func =
      DefineClass(env,
                  "NetworkBinding",
                  {InstanceMethod("send", &NetworkBinding::Send),
                   InstanceMethod("on", &NetworkBinding::On),
                   InstanceMethod("reset", &NetworkBinding::Reset),
                   InstanceMethod("process", &NetworkBinding::Process),
                   InstanceMethod("shutdown", &NetworkBinding::Shutdown),
                   InstanceMethod("hash", &NetworkBinding::Hash)});

    Napi::FunctionReference* constructor = new Napi::FunctionReference();

    // Create a persistent reference to the class constructor. This will allow
    // a function called on a class prototype and a function
    // called on instance of a class to be distinguished from each other.
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("NetworkBinding", func);

    // Store the constructor as the add-on instance data. This will allow this
    // add-on to support multiple instances of itself running on multiple worker
    // threads, as well as multiple instances of itself running in different
    // contexts on the same thread.
    env.SetInstanceData<Napi::FunctionReference>(constructor);
}

/*
void NetworkBinding::New(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    
    // Invoked as constructor: `new MyObject(...)`
    if (info.IsConstructCall()) {
        NetworkBinding* obj = new NetworkBinding();
        obj->Wrap(info.This());
        return info.This();
    }
    // Invoked as function: `MyObject(...)` convert to construct call
    else {
        Napi::Function cons = Napi::Function::New(env, constructor);
        return Napi::NewInstance(cons, 0, nullptr);
    }
}

Napi::FunctionReference NetworkBinding::constructor;
*/

}  // namespace NUClear
