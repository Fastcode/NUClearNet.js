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

NetworkBinding::NetworkBinding(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NetworkBinding>(info) {}

Napi::Value NetworkBinding::Hash(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() > 0 && info[0].IsString()) {
        // Calculate hash
        std::string s = info[0].As<Napi::String>().Utf8Value();
        uint64_t hash = xxhash64(s.c_str(), s.size(), 0x4e55436c);

        // Return hash
        return Napi::Buffer<char>::Copy(env, reinterpret_cast<const char*>(&hash), sizeof(uint64_t)).As<Napi::Value>();
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

    const Napi::Value& arg_hash     = info[0];
    const Napi::Value& arg_payload  = info[1];
    const Napi::Value& arg_target   = info[2];
    const Napi::Value& arg_reliable = info[3];

    uint64_t hash = 0;
    std::vector<uint8_t> payload;
    std::string target = "";
    bool reliable      = false;

    // Read reliability information
    if (arg_reliable.IsBoolean()) {
        reliable = arg_reliable.As<Napi::Boolean>().Value();
    }
    else {
        Napi::TypeError::New(env, "Invalid `reliable` option for send(): expected a boolean")
            .ThrowAsJavaScriptException();
        return;
    }

    // Read target information: if we have a string, use it as the target
    if (arg_target.IsString()) {
        target = arg_target.As<Napi::String>().Utf8Value();
    }
    // Otherwise, we accept null and undefined to mean everybody
    else if (!arg_target.IsUndefined() && !arg_target.IsNull()) {
        Napi::TypeError::New(
            env,
            "Invalid `target` option for send(): expected a string (for targeted), or null/undefined (for untargeted)")
            .ThrowAsJavaScriptException();
        return;
    }

    // Read the data information
    if (arg_payload.IsTypedArray()) {
        Napi::TypedArray typed_array = arg_payload.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer     = typed_array.ArrayBuffer();

        char* data  = reinterpret_cast<char*>(buffer.Data());
        char* start = data + typed_array.ByteOffset();
        char* end   = start + typed_array.ByteLength();

        payload.insert(payload.begin(), start, end);
    }
    else {
        Napi::TypeError::New(env, "Invalid `payload` option for send(): expected a Buffer")
            .ThrowAsJavaScriptException();
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
        Napi::ArrayBuffer buffer     = typed_array.ArrayBuffer();

        uint8_t* data  = reinterpret_cast<uint8_t*>(buffer.Data());
        uint8_t* start = data + typed_array.ByteOffset();
        uint8_t* end   = start + typed_array.ByteLength();

        if (std::distance(start, end) == 8) {
            std::memcpy(&hash, start, 8);
        }
        else {
            Napi::TypeError::New(env, "Invalid `hash` option for send(): provided Buffer length is not 8")
                .ThrowAsJavaScriptException();
            return;
        }
    }
    else {
        Napi::TypeError::New(env, "Invalid `hash` option for send(): expected a string or Buffer")
            .ThrowAsJavaScriptException();
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

void NetworkBinding::OnPacket(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Function to execute on the network thread
    on_packet = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnPacket", 0, 1);

    this->net.set_packet_callback([this](const NUClearNetwork::NetworkTarget& t,
                                         const uint64_t& hash,
                                         const bool& reliable,
                                         std::vector<uint8_t>&& payload) {
        std::string name                     = t.name;
        std::pair<std::string, int16_t> addr = t.target.address();
        on_packet.BlockingCall(
            [name, addr, hash, reliable, p = std::move(payload)](Napi::Env env, Napi::Function js_callback) {
                js_callback.Call({
                    Napi::String::New(env, name),
                    Napi::String::New(env, addr.first),
                    Napi::Number::New(env, addr.second),
                    Napi::Boolean::New(env, reliable),
                    Napi::Buffer<uint8_t>::Copy(env, reinterpret_cast<const uint8_t*>(&hash), sizeof(uint64_t)),
                    Napi::Buffer<uint8_t>::Copy(env, p.data(), p.size()),
                });
            });
    });
}

void NetworkBinding::OnJoin(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Function to execute on the network thread
    on_join = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnJoin", 0, 1);

    this->net.set_join_callback([this](const NUClearNetwork::NetworkTarget& t) {
        std::string name                     = t.name;
        std::pair<std::string, int16_t> addr = t.target.address();
        on_join.BlockingCall([name, addr](Napi::Env env, Napi::Function js_callback) {
            js_callback.Call({
                Napi::String::New(env, t.name),
                Napi::String::New(env, addr.first),
                Napi::Number::New(env, addr.second),
            });
        });
    });
}

void NetworkBinding::OnLeave(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Function to execute on the network thread
    on_leave = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnLeave", 0, 1);

    this->net.set_leave_callback([this](const NUClearNetwork::NetworkTarget& t) {
        std::string name                     = t.name;
        std::pair<std::string, int16_t> addr = t.target.address();
        on_leave.BlockingCall([name, addr](Napi::Env env, Napi::Function js_callback) {
            js_callback.Call({
                Napi::String::New(env, name),
                Napi::String::New(env, addr.first),
                Napi::Number::New(env, addr.second),
            });
        });
    });
}

void NetworkBinding::OnWait(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // Function to execute on the network thread
    on_wait = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnWait", 0, 1);

    this->net.set_next_event_callback([this](const std::chrono::steady_clock::time_point& t) {
        using namespace std::chrono;
        // Add 1 to account for any funky rounding
        int ms = 1 + duration_cast<duration<int, std::milli>>(t - steady_clock::now()).count();
        on_wait.BlockingCall(
            [ms](Napi::Env env, Napi::Function js_callback) { js_callback.Call({Napi::Number::New(env, ms)}); });
    });
}

void NetworkBinding::Reset(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    const Napi::Value& arg_name  = info[0];
    const Napi::Value& arg_group = info[1];
    const Napi::Value& arg_port  = info[2];
    const Napi::Value& arg_mtu   = info[3];

    std::string name     = "";
    std::string group    = "239.226.152.162";
    uint32_t port        = arg_port.IsNumber() ? arg_port.As<Napi::Number>().Uint32Value() : 7447;
    uint32_t network_mtu = arg_mtu.IsNumber() ? arg_mtu.As<Napi::Number>().Uint32Value() : 1500;

    // Multicast Group
    if (arg_group.IsString()) {
        group = arg_group.As<Napi::String>().Utf8Value();
    }
    else {
        Napi::TypeError::New(env, "Invalid `group` option for reset(): multicast group must be a string")
            .ThrowAsJavaScriptException();
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

    // Create empty lambdas for the callbacks, to prevent them from being called
    this->net.set_packet_callback([](const NUClearNetwork::NetworkTarget& t,
                                     const uint64_t& hash,
                                     const bool& reliable,
                                     std::vector<uint8_t>&& payload) {});
    this->net.set_join_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_leave_callback([](const NUClearNetwork::NetworkTarget& t) {});
    this->net.set_next_event_callback([](std::chrono::steady_clock::time_point t) {});

    // Release the thread safe functions
    on_packet.Release();
    on_join.Release();
    on_leave.Release();
    on_wait.Release();
}

void NetworkBinding::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env,
                                      "NetworkBinding",
                                      {InstanceMethod<&NetworkBinding::Send>(
                                           "send",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::OnPacket>(
                                           "onPacket",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::OnJoin>(
                                           "onJoin",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::OnLeave>(
                                           "onLeave",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::OnWait>(
                                           "onWait",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Reset>(
                                           "reset",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Process>(
                                           "process",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Shutdown>(
                                           "shutdown",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Hash>(
                                           "hash",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Destroy>(
                                           "destroy",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable))});

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
