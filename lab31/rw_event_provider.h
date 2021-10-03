#ifndef LAB31_RW_EVENT_PROVIDER_H
#define LAB31_RW_EVENT_PROVIDER_H

#include "poll_event.h"

namespace rwev {
    using namespace pollev;

    using RWEventListener = std::function<void(short)>;

    class RWEventSource {
    private:
        PollEventProvider source;
        int fd;
    public:
        RWEventSource() {}

        RWEventSource(int fd, short event, PollEventGenerator& generator): fd {fd}, source {generator.add(fd, event)} {}
        RWEventSource(int fd, PollEventGenerator& generator): fd {fd}, source {generator.add(fd, 0)} {}
        ~RWEventSource() {}

        RWEventSource(const RWEventSource&)=delete;
        RWEventSource& operator=(const RWEventSource&)=delete;

        RWEventSource(RWEventSource&& other): source {std::move(other.source)}, fd {other.fd} {}
        RWEventSource& operator=(RWEventSource&& other) {
            source = std::move(other.source);
            fd = other.fd;

            return *this;
        }

        ssize_t write(const void* buf, size_t count) {
            return ::write(fd, buf, count);
        }

        ssize_t read(void* buf, size_t count) {
            return ::read(fd, buf, count);
        }

        void addRWEventListener(const RWEventListener& listener) {
            source.addListener(listener);
        }

        void addRemoveListener(const RemoveListener& listener) {
            source.addRemoveListener(listener);
        }

        void setRWEvent(short event) {
            source.setEvent(event);
        }

        void unsetRWEvent(short event) {
            source.unsetEvent(event);
        }
    };

    class RWEventGenerator {
    private:
        PollEventGenerator generator;
    public:
        RWEventGenerator(const PollEventGenerator& generator): generator {generator} {}

        RWEventSource add(int fd, short event) {
            return RWEventSource {fd, event, generator};
        }

        RWEventSource add(int fd) {
            return RWEventSource {fd, generator};
        }
    };

    using RWProviderAddedListener = std::function<void(RWEventSource&&)>;
    using ErrorListener = std::function<void(int)>; //(error)
}

#endif // !LAB31_RW_EVENT_PROVIDER_H