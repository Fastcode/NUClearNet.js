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
#include "nuclear/src/util/serialise/xxhash.hpp"

namespace NUClear {

using extension::network::NUClearNetwork;
using util::serialise::xxhash64;

NetworkBinding::NetworkBinding(const Napi::CallbackInfo& info): Napi::ObjectWrap<NetworkBinding>(info) {}

Napi::Value NetworkBinding::Hash(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() > 0 && info[0].IsString()) {
        // Calculate hash
        std::string s = info[0].As<Napi::String>().Utf8Value();
        uint64_t hash = xxhash64(s.c_str(), s.size(), 0x4e55436c);

        // Return hash
        return
            Napi::Buffer<char>::Copy(env, reinterpret_cast<const char*>(&hash), sizeof(uint64_t)).As<Napi::Value>();
    }
    else {
        Napi::TypeError::New(env, "Invalid input for hash(): expected a string").ThrowAsJavaScriptException();
        return env.Null();
    }
}

void NetworkBinding::Send(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4) {
        Napi::TypeError::New(env, "Expected 4 arguments, got fewer").ThrowAsJavaScriptException();
        return;
    }

    const Napi::Value& arg_hash = info[0];
    const Napi::Value& arg_payload = info[1];
    const Napi::Value& arg_target = info[2];
    const Napi::Value& arg_reliable = info[3];

    uint64_t hash = 0;
    std::vector<uint8_t> payload;
    std::string target = "";
    bool reliable      = false;

    // Read reliability information
    if (arg_reliable.IsBoolean()) {
        reliable = arg_reliable.As<Napi::Boolean>().Value();
    } else {
        Napi::TypeError::New(env, "Invalid `reliable` option for send(): expected a boolean").ThrowAsJavaScriptException();
        return;
    }

    // Read target information: if we have a string, use it as the target
    if (arg_target.IsString()) {
        target = arg_target.As<Napi::String>().Utf8Value();
    }
    // Otherwise, we accept null and undefined to mean everybody
    else if (!arg_target.IsUndefined() && !arg_target.IsNull()) {
        Napi::TypeError::New(env, "Invalid `target` option for send(): expected a string (for targeted), or null/undefined (for untargeted)").ThrowAsJavaScriptException();
        return;
    }

    // Read the data information
    if (arg_payload.IsTypedArray()) {
        Napi::TypedArray typed_array = arg_payload.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer = typed_array.ArrayBuffer();

        char* data = reinterpret_cast<char*>(buffer.Data());
        char* start = data + typed_array.ByteOffset();
        char* end = start + typed_array.ByteLength();

        payload.insert(payload.begin(), start, end);
    }
    else {
        Napi::TypeError::New(env, "Invalid `payload` option for send(): expected a Buffer").ThrowAsJavaScriptException();
        return;
    }

    // If we have a string, apply XXHash to get the hash
    if (arg_hash.IsString()) {
        std::string s = arg_hash.As<Napi::String>().Utf8Value();
        hash          = xxhash64(s.c_str(), s.size(), 0x4e55436c);
    }
    // Otherwise try to interpret it as a buffer that contains the hash
    else if (arg_hash.IsTypedArray()) {
        Napi::TypedArray typed_array = arg_hash.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer = typed_array.ArrayBuffer();

        uint8_t* data = reinterpret_cast<uint8_t*>(buffer.Data());
        uint8_t* start = data + typed_array.ByteOffset();
        uint8_t* end = start + typed_array.ByteLength();

        if (std::distance(start, end) == 8) {
            std::memcpy(&hash, start, 8);
        }
        else {
            Napi::TypeError::New(env, "Invalid `hash` option for send(): provided Buffer length is not 8").ThrowAsJavaScriptException();
            return;
        }
    } else {
        Napi::TypeError::New(env, "Invalid `hash` option for send(): expected a string or Buffer").ThrowAsJavaScriptException();
        return;
    }

    // Perform the send
    try {
        this->net.send(hash, payload, target, reliable);
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::On(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info[0].IsString() && info[1].IsFunction()) {
        std::string event    = info[0].As<Napi::String>().Utf8Value();

        // `ThreadSafeCallback` is from the napi-thread-safe-callback npm package. It allows for running a JS callback
        // from a thread that isn't the main addon thread (like where the NUClearNet packet callbacks are called from).
        // Similar thread-safe callback functionality has been added to NAPI natively, but it's still experimental at
        // the time of writing.
        auto cb = std::make_shared<ThreadSafeCallback>(info[1].As<Napi::Function>());

        if (event == "packet") {
            this->net.set_packet_callback([cb = std::move(cb)](
                const NUClearNetwork::NetworkTarget& t, const uint64_t& hash, const bool& reliable, std::vector<uint8_t>&& payload) {

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
                }
                address = c;

                cb->call([name, address, port, reliable, hash, payload](Napi::Env env, std::vector<napi_value> &args) {
                    args = {
                        Napi::String::New(env, name),
                        Napi::String::New(env, address),
                        Napi::Number::New(env, port),
                        Napi::Boolean::New(env, reliable),
                        Napi::Buffer<uint8_t>::Copy(env, reinterpret_cast<const uint8_t*>(&hash), sizeof(uint64_t)),
                        Napi::Buffer<uint8_t>::Copy(env, payload.data(), payload.size())
                    };
                });
            });
        }
        else if (event == "join" || event == "leave") {
            auto f = [cb = std::move(cb)](const NUClearNetwork::NetworkTarget& t) {
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

                    default:
                        // The system has a corrupted network peer record, but we can't throw to JS from here, since we
                        // don't have an env object. cb->callError(string) is available from the
                        // napi-thread-safe-callback library, but that requires changing the callback signature on the
                        // JS side to accept a potential error as the first argument. This would be a breaking change,
                        // but we can do it if deemed necessary, and update all users of nuclearnet.js.
                        return;
                }
                address = c;

                cb->call([name, address, port](Napi::Env env, std::vector<napi_value> &args) {
                    args = {
                        Napi::String::New(env, name),
                        Napi::String::New(env, address),
                        Napi::Number::New(env, port),
                    };
                });
            };

            if (event == "join") {
                this->net.set_join_callback(std::move(f));
            }
            else {
                this->net.set_leave_callback(std::move(f));
            }
        }
        else if (event == "wait") {
            this->net.set_next_event_callback([cb = std::move(cb)](std::chrono::steady_clock::time_point t) {
                using namespace std::chrono;
                int ms = duration_cast<std::chrono::duration<int, std::milli>>(t - steady_clock::now()).count();
                ms++;  // Add 1 to account for any funky rounding

                cb->call([ms](Napi::Env env, std::vector<napi_value> &args) {
                    args = {Napi::Number::New(env, ms)};
                });
            });
        }
        else {
            Napi::TypeError::New(env, "Invalid `eventName` argument for on(): expected one of 'packet', 'join', 'leave', or 'wait'").ThrowAsJavaScriptException();
            return;
        }
    }
    else {
        Napi::TypeError::New(env, "Invalid arguments for on(): expected an event name (string) and a callback (function)").ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Reset(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    const Napi::Value& arg_name = info[0];
    const Napi::Value& arg_group = info[1];
    const Napi::Value& arg_port = info[2];
    const Napi::Value& arg_mtu = info[3];

    std::string name     = "";
    std::string group    = "239.226.152.162";
    uint32_t port        = arg_port.IsNumber() ? arg_port.As<Napi::Number>().Uint32Value() : 7447;
    uint32_t network_mtu = arg_mtu.IsNumber() ? arg_mtu.As<Napi::Number>().Uint32Value() : 1500;

    // Multicast Group
    if (arg_group.IsString()) {
        group = arg_group.As<Napi::String>().Utf8Value();
    }
    else {
        Napi::TypeError::New(env, "Invalid `group` option for reset(): multicast group must be a string").ThrowAsJavaScriptException();
        return;
    }

    // Name Group
    if (arg_name.IsString()) {
        name = arg_name.As<Napi::String>().Utf8Value();
    }
    else {
        Napi::Error::New(env, "Invalid `name` option for reset(): name must be a string").ThrowAsJavaScriptException();
        return;
    }

    // Perform the reset
    try {
        this->net.reset(name, group, port, network_mtu);

        // NetworkListener extends AsyncProgressWorker, which will automatically
        // destruct itself when done (i.e. when Execute() returns and OnOK() or
        // OnError() are called and return)
        auto asyncWorker = new NetworkListener(env, this);

#ifdef _WIN32
        // Keep track of the NetworkListener notifier, so we can signal it to exit WSAWaitForMultipleEvents()
        this->listenerNotifier = asyncWorker->notifier;
#endif

        // Queue the worker
        asyncWorker->Queue();
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Process(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Perform the process function
    try {
        this->net.process();
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Shutdown(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Perform the shutdown function
    try {
        this->net.shutdown();
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Destroy(const Napi::CallbackInfo& info) {
    // Set destroyed, to exit the read loop in the network listener
    this->destroyed = true;

#ifdef _WIN32
    // Signal the network listener notifier, to exit WSAWaitForMultipleEvents()
    WSASetEvent(this->listenerNotifier);
#endif

    // Replace the ThreadSafeCallback instances to clean up the extra threads they created
    this->net.set_packet_callback([](const NUClearNetwork::NetworkTarget& t, const uint64_t& hash, const bool& reliable, std::vector<uint8_t>&& payload) {});
    this->net.set_join_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_leave_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_next_event_callback([](std::chrono::steady_clock::time_point t) {});
}

void NetworkBinding::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func =
      DefineClass(env,
                  "NetworkBinding",
                  {InstanceMethod<&NetworkBinding::Send>("send", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::On>("on", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::Reset>("reset", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::Process>("process", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::Shutdown>("shutdown", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::Hash>("hash", static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                   InstanceMethod<&NetworkBinding::Destroy>("destroy", static_cast<napi_property_attributes>(napi_writable | napi_configurable))});

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
    //
    // By default, the value set on the environment here will be destroyed when
    // the add-on is unloaded using the `delete` operator, but it is also
    // possible to supply a custom deleter.
    env.SetInstanceData<Napi::FunctionReference>(constructor);
}

}  // namespace NUClear
