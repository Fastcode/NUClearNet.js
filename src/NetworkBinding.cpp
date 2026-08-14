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

#include <set>
#include <string>

#include "NetworkListener.hpp"
#include "nuclear/src/nuclearnet/Log.hpp"
#include "nuclear/src/util/serialise/xxhash.hpp"

namespace NUClear {

using network::NUClearNet;
using network::PeerInfo;
using util::network::sock_t;
using util::serialise::xxhash64;

namespace {

uint64_t hash_from_value(Napi::Env env, const Napi::Value& arg_hash) {
    if (arg_hash.IsString()) {
        std::string s = arg_hash.As<Napi::String>().Utf8Value();
        return xxhash64(s.c_str(), s.size(), 0x4e55436c);
    }

    if (arg_hash.IsTypedArray()) {
        Napi::TypedArray typed_array = arg_hash.As<Napi::TypedArray>();
        Napi::ArrayBuffer buffer     = typed_array.ArrayBuffer();

        uint8_t* data  = reinterpret_cast<uint8_t*>(buffer.Data());
        uint8_t* start = data + typed_array.ByteOffset();
        uint8_t* end   = start + typed_array.ByteLength();

        if (std::distance(start, end) != 8) {
            Napi::TypeError::New(env, "Invalid hash Buffer: expected length 8").ThrowAsJavaScriptException();
            return 0;
        }

        uint64_t hash = 0;
        std::memcpy(&hash, start, 8);
        return hash;
    }

    Napi::TypeError::New(env, "Invalid hash: expected a string or Buffer").ThrowAsJavaScriptException();
    return 0;
}

std::set<uint64_t> hashes_from_array(Napi::Env env, const Napi::Value& arg) {
    std::set<uint64_t> hashes;

    if (!arg.IsArray()) {
        Napi::TypeError::New(env, "setSubscriptions(): expected an array of hash Buffers")
            .ThrowAsJavaScriptException();
        return hashes;
    }

    Napi::Array array = arg.As<Napi::Array>();
    for (uint32_t i = 0; i < array.Length(); ++i) {
        const Napi::Value& item = array[i];
        if (!item.IsBuffer() && !item.IsTypedArray()) {
            Napi::TypeError::New(env, "setSubscriptions(): each entry must be an 8-byte Buffer")
                .ThrowAsJavaScriptException();
            return {};
        }

        uint64_t hash = hash_from_value(env, item);
        if (env.IsExceptionPending()) {
            return {};
        }
        hashes.insert(hash);
    }

    return hashes;
}

network::LogLevel log_level_from_value(Napi::Env env, const Napi::Value& arg) {
    if (arg.IsNumber()) {
        const int level = arg.As<Napi::Number>().Int32Value();
        if (level >= static_cast<int>(network::LogLevel::Off)
            && level <= static_cast<int>(network::LogLevel::Trace)) {
            return static_cast<network::LogLevel>(level);
        }
        Napi::TypeError::New(env, "setLogLevel(): level must be 0 (off) through 5 (trace)")
            .ThrowAsJavaScriptException();
        return network::LogLevel::Off;
    }

    if (arg.IsString()) {
        const std::string s = arg.As<Napi::String>().Utf8Value();
        if (s == "off") {
            return network::LogLevel::Off;
        }
        if (s == "error") {
            return network::LogLevel::Error;
        }
        if (s == "warn") {
            return network::LogLevel::Warn;
        }
        if (s == "info") {
            return network::LogLevel::Info;
        }
        if (s == "debug") {
            return network::LogLevel::Debug;
        }
        if (s == "trace") {
            return network::LogLevel::Trace;
        }
        Napi::TypeError::New(env,
                             "setLogLevel(): expected off, error, warn, info, debug, or trace")
            .ThrowAsJavaScriptException();
        return network::LogLevel::Off;
    }

    Napi::TypeError::New(env, "setLogLevel(): expected a number or string").ThrowAsJavaScriptException();
    return network::LogLevel::Off;
}

}  // namespace

NetworkBinding::NetworkBinding(const Napi::CallbackInfo& info) : Napi::ObjectWrap<NetworkBinding>(info) {
    this->net.set_socket_change_callback([this]() { this->request_listener_restart(); });
}

NetworkBinding::~NetworkBinding() {
    // The log handler is global and captures this, so make sure it can't outlive us even if destroy() was missed
    network::NUClearNet::set_log_handler(nullptr);
}

void NetworkBinding::stop_listener() {
    ++this->listener_generation;

#ifdef _WIN32
    if (this->listener_notifier != WSA_INVALID_EVENT) {
        WSASetEvent(this->listener_notifier);
    }
#endif
}

void NetworkBinding::start_listener(Napi::Env env) {
    this->stop_listener();

    auto* asyncWorker = new NetworkListener(env, this);

#ifdef _WIN32
    this->listener_notifier = asyncWorker->notifier;
#endif

    asyncWorker->Queue();
}

void NetworkBinding::request_listener_restart() {
    if (this->destroyed) {
        return;
    }

    if (this->listener_restart) {
        this->listener_restart.NonBlockingCall([this](Napi::Env env, Napi::Function) { this->start_listener(env); });
    }
}

Napi::Value NetworkBinding::Hash(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() > 0 && info[0].IsString()) {
        std::string s = info[0].As<Napi::String>().Utf8Value();
        uint64_t hash = xxhash64(s.c_str(), s.size(), 0x4e55436c);

        return Napi::Buffer<char>::Copy(env, reinterpret_cast<const char*>(&hash), sizeof(uint64_t)).As<Napi::Value>();
    }

    Napi::TypeError::New(env, "Invalid input for hash(): expected a string").ThrowAsJavaScriptException();
    return env.Null();
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

    uint64_t hash = hash_from_value(env, arg_hash);
    if (env.IsExceptionPending()) {
        return;
    }

    std::vector<uint8_t> payload;
    std::string target = "";
    bool reliable      = false;

    if (arg_reliable.IsBoolean()) {
        reliable = arg_reliable.As<Napi::Boolean>().Value();
    }
    else {
        Napi::TypeError::New(env, "Invalid `reliable` option for send(): expected a boolean")
            .ThrowAsJavaScriptException();
        return;
    }

    if (arg_target.IsString()) {
        target = arg_target.As<Napi::String>().Utf8Value();
    }
    else if (!arg_target.IsUndefined() && !arg_target.IsNull()) {
        Napi::TypeError::New(
            env,
            "Invalid `target` option for send(): expected a string (for targeted), or null/undefined (for untargeted)")
            .ThrowAsJavaScriptException();
        return;
    }

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

    try {
        this->net.send(hash, payload.data(), payload.size(), target, reliable);
    }
    catch (const std::exception& ex) {
        if (network::should_log(network::LogLevel::Error)) {
            network::log(network::LogLevel::Error, "binding", std::string("send failed: ") + ex.what());
        }
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::SetLogLevel(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "setLogLevel(): expected a level").ThrowAsJavaScriptException();
        return;
    }

    network::LogLevel level = log_level_from_value(env, info[0]);
    if (env.IsExceptionPending()) {
        return;
    }

    network::NUClearNet::set_log_level(level);
}

void NetworkBinding::OnPacket(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    on_packet = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnPacket", 0, 1);

    this->net.set_packet_callback([this](const sock_t& source,
                                         const std::string& peer_name,
                                         uint64_t hash,
                                         bool reliable,
                                         std::vector<uint8_t>&& payload) {
        std::pair<std::string, in_port_t> addr = source.address();
        on_packet.BlockingCall(
            [peer_name, addr, hash, reliable, p = std::move(payload)](Napi::Env env, Napi::Function js_callback) {
                js_callback.Call({
                    Napi::String::New(env, peer_name),
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

    on_join = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnJoin", 0, 1);

    this->net.set_join_callback([this](const PeerInfo& peer) {
        std::pair<std::string, in_port_t> addr = peer.address.address();
        on_join.BlockingCall([name = peer.name, addr](Napi::Env env, Napi::Function js_callback) {
            js_callback.Call({
                Napi::String::New(env, name),
                Napi::String::New(env, addr.first),
                Napi::Number::New(env, addr.second),
            });
        });
    });
}

void NetworkBinding::OnLeave(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    on_leave = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnLeave", 0, 1);

    this->net.set_leave_callback([this](const PeerInfo& peer) {
        std::pair<std::string, in_port_t> addr = peer.address.address();
        on_leave.BlockingCall([name = peer.name, addr](Napi::Env env, Napi::Function js_callback) {
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

    on_wait = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnWait", 0, 1);

    this->net.set_event_callback([this](const std::chrono::steady_clock::time_point& t) {
        using namespace std::chrono;
        int ms = 1 + duration_cast<duration<int, std::milli>>(t - steady_clock::now()).count();
        on_wait.BlockingCall(
            [ms](Napi::Env env, Napi::Function js_callback) { js_callback.Call({Napi::Number::New(env, ms)}); });
    });
}

void NetworkBinding::OnLog(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    on_log = Napi::ThreadSafeFunction::New(env, info[0].As<Napi::Function>(), "OnLog", 0, 1);

    // Hand the native logs to the JavaScript logger so they come out the same way as our own messages.
    // This is a global handler, so the most recently created network wins if there is more than one.
    network::NUClearNet::set_log_handler(
        [this](network::LogLevel level, const char* component, const std::string& message) {
            on_log.BlockingCall(
                [level, component = std::string(component), message](Napi::Env env, Napi::Function js_callback) {
                    js_callback.Call({
                        Napi::Number::New(env, static_cast<int>(level)),
                        Napi::String::New(env, component),
                        Napi::String::New(env, message),
                    });
                });
        });
}

void NetworkBinding::Reset(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    const Napi::Value& arg_name  = info[0];
    const Napi::Value& arg_group = info[1];
    const Napi::Value& arg_port  = info[2];
    const Napi::Value& arg_mtu   = info[3];

    network::NetworkConfig config;
    config.announce_address = "239.226.152.162";
    config.announce_port    = arg_port.IsNumber() ? static_cast<in_port_t>(arg_port.As<Napi::Number>().Uint32Value())
                                                  : static_cast<in_port_t>(7447);
    config.mtu              = arg_mtu.IsNumber() ? static_cast<uint16_t>(arg_mtu.As<Napi::Number>().Uint32Value()) : 1500;

    if (arg_group.IsString()) {
        config.announce_address = arg_group.As<Napi::String>().Utf8Value();
    }
    else {
        Napi::TypeError::New(env, "Invalid `group` option for reset(): multicast group must be a string")
            .ThrowAsJavaScriptException();
        return;
    }

    if (arg_name.IsString()) {
        config.name = arg_name.As<Napi::String>().Utf8Value();
    }
    else {
        Napi::Error::New(env, "Invalid `name` option for reset(): name must be a string").ThrowAsJavaScriptException();
        return;
    }

    try {
        this->net.reset(config);

        if (!this->listener_restart) {
            this->listener_restart =
                Napi::ThreadSafeFunction::New(env,
                                              Napi::Function::New(env, [this](const Napi::CallbackInfo& info) {
                                                  this->start_listener(info.Env());
                                              }),
                                              "ListenerRestart",
                                              0,
                                              1);
        }

        this->start_listener(env);
    }
    catch (const std::exception& ex) {
        if (network::should_log(network::LogLevel::Error)) {
            network::log(network::LogLevel::Error, "binding", std::string("reset failed: ") + ex.what());
        }
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Process(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    try {
        this->net.process();
    }
    catch (const std::exception& ex) {
        if (network::should_log(network::LogLevel::Error)) {
            network::log(network::LogLevel::Error, "binding", std::string("process failed: ") + ex.what());
        }
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Shutdown(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    try {
        this->stop_listener();
        this->net.shutdown();
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::AddSubscription(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "addSubscription(): expected a hash Buffer").ThrowAsJavaScriptException();
        return;
    }

    uint64_t hash = hash_from_value(env, info[0]);
    if (env.IsExceptionPending()) {
        return;
    }

    try {
        this->net.add_subscription(hash);
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::SetSubscriptions(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(env, "setSubscriptions(): expected an array of hash Buffers").ThrowAsJavaScriptException();
        return;
    }

    std::set<uint64_t> hashes = hashes_from_array(env, info[0]);
    if (env.IsExceptionPending()) {
        return;
    }

    try {
        this->net.set_subscriptions(hashes);
    }
    catch (const std::exception& ex) {
        Napi::Error::New(env, ex.what()).ThrowAsJavaScriptException();
    }
}

void NetworkBinding::Destroy(const Napi::CallbackInfo& info) {
    this->destroyed = true;

    this->stop_listener();

    // Put the native logs back on stderr before releasing the function they were going to
    network::NUClearNet::set_log_handler(nullptr);

    this->net.set_socket_change_callback([]() {});
    this->net.set_packet_callback(
        [](const sock_t&, const std::string&, uint64_t, bool, std::vector<uint8_t>&&) {});
    this->net.set_join_callback([](const PeerInfo&) {});
    this->net.set_leave_callback([](const PeerInfo&) {});
    this->net.set_event_callback([](std::chrono::steady_clock::time_point) {});

    on_packet.Release();
    on_join.Release();
    on_leave.Release();
    on_wait.Release();
    if (this->on_log) {
        this->on_log.Release();
    }
    if (this->listener_restart) {
        this->listener_restart.Release();
    }
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
                                       InstanceMethod<&NetworkBinding::AddSubscription>(
                                           "addSubscription",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::SetSubscriptions>(
                                           "setSubscriptions",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::SetLogLevel>(
                                           "setLogLevel",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::OnLog>(
                                           "onLog",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable)),
                                       InstanceMethod<&NetworkBinding::Destroy>(
                                           "destroy",
                                           static_cast<napi_property_attributes>(napi_writable | napi_configurable))});

    Napi::FunctionReference* constructor = new Napi::FunctionReference();

    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);

    exports.Set("NetworkBinding", func);

    env.SetInstanceData<Napi::FunctionReference>(constructor);
}

}  // namespace NUClear
