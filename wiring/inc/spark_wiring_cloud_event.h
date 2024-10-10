/*
 * Copyright (c) 2024 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <cstring>

#include "spark_wiring_stream.h"
#include "spark_wiring_error.h"

#include "system_cloud_event.h"

namespace particle {

class Buffer;
class Variant;

class CloudEvent: public Stream {
public:
    enum Status {
        NEW,
        SENDING,
        SENT,
        FAILED
    };

    typedef void (*OnStatusChange)(CloudEvent event, void* arg);
    typedef std::function<void(CloudEvent event)> OnStatusChangeFn;

    CloudEvent() :
            ev_(nullptr) {
        cloud_event_create(&ev_, nullptr /* reserved */);
    }

    explicit CloudEvent(const char* name) :
            CloudEvent() {
        this->name(name);
    }

    CloudEvent(const char* name, const char* data) :
            CloudEvent(name) {
        this->data(data);
    }

    CloudEvent(const char* name, const char* data, size_t size, ContentType type) :
            CloudEvent(name) {
        this->data(data, size, type);
    }

    CloudEvent(const char* name, const Buffer& data, ContentType type) :
            CloudEvent(name) {
        this->data(data, type);
    }

    CloudEvent(const char* name, const Variant& data) :
            CloudEvent(name) {
        this->data(data);
    }

    CloudEvent(const CloudEvent& event) :
            ev_(event.ev_) {
        cloud_event_add_ref(ev_, nullptr /* reserved */);
    }

    CloudEvent(CloudEvent&& event) :
            ev_(event.ev_) {
        event.ev_ = nullptr;
    }

    ~CloudEvent() {
        if (ev_) {
            cloud_event_release(ev_, nullptr /* reserved */);
        }
    }

    CloudEvent& name(const char* name) {
        if (ev_) {
            cloud_event_set_name(ev_, name, nullptr /* reserved */);
        }
        return *this;
    }

    const char* name() const {
        if (!ev_) {
            return "";
        }
        return cloud_event_get_name(ev_, nullptr /* reserved */);
    }

    CloudEvent& contentType(ContentType type);
    ContentType contentType() const;

    CloudEvent& data(const char* data) {
        return this->data(data, std::strlen(data));
    }

    CloudEvent& data(const char* data, size_t size);

    CloudEvent& data(const char* data, size_t size, ContentType type) {
        if (ev_) {
            this->data(data, size);
            contentType(type);
        }
        return *this;
    }

    CloudEvent& data(const Buffer& data) {
        return this->data(data.data(), data.size());
    }

    CloudEvent& data(const Buffer& data, ContentType type) {
        return this->data(data.data(), data.size(), type);
    }

    CloudEvent& data(const Variant& data);

    Buffer data() const;
    Variant dataAsVariant() const;

    CloudEvent& size(size_t size) {
        if (ev_) {
            cloud_event_set_size(ev_, size, nullptr /* reserved */);
        }
        return *this;
    }

    size_t size() const {
        if (!ev_) {
            return 0;
        }
        return cloud_event_get_size(ev_, nullptr /* reserved */);
    }

    CloudEvent& onStatusChange(OnStatusChange callback, void* arg = nullptr);
    CloudEvent& onStatusChange(OnStatusChangeFn callback);

    Status status() const {
        if (!ev_) {
            return Status::FAILED;
        }
        int s = cloud_event_get_status(ev_, nullptr /* reserved */);
        return static_cast<Status>(s);
    }

    bool isSent() const {
        return status() == Status::SENT;
    }

    bool ok() const {
        return status() != Status::FAILED;
    }
    
    int save(const char* filename);
    int save(int fd);

    static CloudEvent load(const char* filename);
    static CloudEvent load(int fd);

    int read() override;
    size_t readBytes(char* data, size_t size) override;
    int peek() override;

    int available() override {
        if (!ev_) {
            return 0;
        }
        return size() - pos();
    }

    size_t write(uint8_t b) override {
        return write(&b, 1);
    }

    size_t write(const uint8_t* data, size_t size) override;

    void flush() override {
    }

    CloudEvent& pos(size_t pos) {
        if (ev_) {
            cloud_event_seek(ev_, pos, nullptr /* reserved */);
        }
        return *this;
    }

    size_t pos() const;

    int error() const {
        if (!ev_) {
            return Error::NO_MEMORY;
        }
        return cloud_event_get_error(ev_, nullptr /* reserved */);
    }

    void clearError() {
        if (ev_) {
            cloud_event_clear_error(ev_, nullptr /* reserved */);
        }
    }

    CloudEvent& operator=(CloudEvent event) {
        swap(*this, event);
        return *this;
    }

    friend void swap(CloudEvent& event1, CloudEvent& event2) {
        auto ev = event1.ev_;
        event1.ev_ = event2.ev_;
        event2.ev_ = ev;
    }

private:
    cloud_event* ev_;
};

} // namespace particle
