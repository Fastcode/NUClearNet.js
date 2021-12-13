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

Napi::Value NetworkBinding::Hash(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() > 0 && info[0].IsString()) {
        // Calculate hash
        std::string s = info[0].As<Napi::String>().Utf8Value();
        uint64_t hash = XXH64(s.c_str(), s.size(), 0x4e55436c);

        // Return hash
        return
            Napi::Buffer<char>::Copy(env, reinterpret_cast<const char*>(&hash), sizeof(uint64_t)).As<Napi::Value>();
    }
    else {
        Napi::Error::New(env, "Can only hash strings").ThrowAsJavaScriptException();
        return env.Null();
    }
}

void NetworkBinding::Send(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 4) {
        Napi::Error::New(env, "Expected 4 arguments, got fewer").ThrowAsJavaScriptException();
        return;
    }

    const Napi::Value& hash_arg = info[0];
    const Napi::Value& payload_arg = info[1];
    const Napi::Value& target_arg = info[2];
    const Napi::Value& reliable_arg = info[3];

    uint64_t hash = 0;
    std::vector<char> payload;
    std::string target = "";
    bool reliable      = false;

    // Read reliability information
    if (reliable_arg.IsBoolean()) {
        reliable = reliable_arg.As<Napi::Boolean>().Value();
    } else {
        Napi::Error::New(env, "Invalid `reliable` for the message, expected boolean").ThrowAsJavaScriptException();
        return;
    }

    // Read target information
    // If we have a string here use it
    if (target_arg.IsString()) {
        target = target_arg.As<Napi::String>().Utf8Value();
    }
    // Otherwise, we accept null and undefined to mean everybody
    else if (!target_arg.IsUndefined() && !target_arg.IsNull()) {
        Napi::Error::New(env, "Invalid target for the message").ThrowAsJavaScriptException();
        return;
    }

    // Read the data information
    if (payload_arg.IsTypedArray()) {
        Napi::TypedArray typed_array = payload_arg.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer = typed_array.ArrayBuffer();

        char* data = reinterpret_cast<char*>(buffer.Data());
        char* start = data + typed_array.ByteOffset();
        char* end = start + typed_array.ByteLength();

        payload.insert(payload.begin(), start, end);
    }
    else {
        Napi::Error::New(env, "Provided data to send was not a readable").ThrowAsJavaScriptException();
        return;
    }

    // If we have a string XXHash to get the hash
    if (hash_arg.IsString()) {
        std::string s = hash_arg.As<Napi::String>().Utf8Value();
        hash          = XXH64(s.c_str(), s.size(), 0x4e55436c);
    }
    // Otherwise try to interpret it as a hash
    else {
        Napi::TypedArray typed_array = hash_arg.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer = typed_array.ArrayBuffer();

        uint8_t* data = reinterpret_cast<uint8_t*>(buffer.Data());
        uint8_t* start = data + typed_array.ByteOffset();
        uint8_t* end = start + typed_array.ByteLength();

        if (std::distance(start, end) == 8) {
            std::memcpy(&hash, start, 8);
        }
        else {
            Napi::Error::New(env, "Invalid hash object").ThrowAsJavaScriptException();
            return;
        }
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

        // ThreadSafeCallback is from the napi-thread-safe-callback library, it allows for running a JS callback from a
        // thread that isn't the main addon thread (like where the NUClearNet packet callbacks are called from).
        // Similar thread-safe callback functionality has been added to NAPI natively, but it's still experimental at
        // the time of writing.
        auto cb = std::make_shared<ThreadSafeCallback>(info[1].As<Napi::Function>());

        if (event == "packet") {
            this->net.set_packet_callback([cb = std::move(cb)](
                const NUClearNetwork::NetworkTarget& t, const uint64_t& hash, const bool& reliable, std::vector<char>&& payload) {

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
                        Napi::Buffer<char>::Copy(env, reinterpret_cast<const char*>(&hash), sizeof(uint64_t)),
                        Napi::Buffer<char>::Copy(env, payload.data(), payload.size())
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
                        // JS side to accept a potential error as the first argument.
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
    }
    else {
        Napi::Error::New(env, "on expects a string event name and a function").ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Reset(const Napi::CallbackInfo& info) {
    // info[0] = name
    // info[1] = mutlicast group
    // info[2] = port
    // info[3] = mtu

    Napi::Env env = info.Env();

    std::string name     = "";
    std::string group    = "239.226.152.162";
    uint32_t port        = info[2].IsNumber() ? info[2].As<Napi::Number>().Uint32Value() : 7447;
    uint32_t network_mtu = info[3].IsNumber() ? info[3].As<Napi::Number>().Uint32Value() : 1500;

    // Multicast Group
    if (info[1].IsString()) {
        group = info[1].As<Napi::String>().Utf8Value();
    }
    else {
        Napi::Error::New(env, "Multicast group must be a string").ThrowAsJavaScriptException();
        return;
    }

    // Name Group
    if (info[0].IsString()) {
        name = info[0].As<Napi::String>().Utf8Value();
    }
    else {
        Napi::Error::New(env, "Name must be a string").ThrowAsJavaScriptException();
        return;
    }

    // Perform the reset
    try {
        this->net.reset(name, group, port, network_mtu);

        // NetworkListener extends AsyncProgressWorker, which will automatically
        // destruct itself when done (i.e. when Execute() returns and OnOK() or
        // OnError() are called and return)
        auto asyncWorker = new NetworkListener(env, this);

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
    // Replace the ThreadSafeCallback instances to clean up the extra threads they created
    this->net.set_packet_callback([](const NUClearNetwork::NetworkTarget& t, const uint64_t& hash, const bool& reliable, std::vector<char>&& payload) {});
    this->net.set_join_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_leave_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_next_event_callback([](std::chrono::steady_clock::time_point t) {});
}

void NetworkBinding::Init(Napi::Env env, Napi::Object exports) {
    // Napi::HandleScope scope(env);

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
