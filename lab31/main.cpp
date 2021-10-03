#include <cstdlib>

#include "poll_event.h"
#include "rw_event_provider.h"
#include "server_event_provider.h"
#include "utils.h"

#include <fcntl.h>
#include <unistd.h>

#include "client_event_provider.h"

class RWEventClosure {
private:
    std::shared_ptr<rwev::RWEventSource> source;
    char buf[256];
    size_t size;
public:
    RWEventClosure(const std::shared_ptr<RWEventSource>& source): source {source} {}

    void operator() (short event) {
        if ((event & POLLIN) == POLLIN) {
            if ((size = source->read(buf, sizeof(buf) - 1)) < 0) {
                std::cout << utils::err_message("read") << std::endl;
                auto temp = std::move(source);
                return;
            }

            if (size == 0) {
                std::cout << "Connection closed\n";
                auto temp = std::move(source);
                return;
            }
            
            buf[size] = '\0';
            std::cout << "Read: " <<  buf;
            source->setRWEvent(POLLOUT);
        }

        if ((event & POLLOUT) == POLLOUT) {
            if (source->write(buf, size) < 0) {
                std::cout << utils::err_message("write") << std::endl;
                auto temp = std::move(source);
                return;
            }
            std::cout << "Write: " <<  buf;
            size = 0;
            buf[0] = '\0';
            source->unsetRWEvent(POLLOUT);
        }
    }
};

int main() {
    utils::SocketAddress address;
    utils::parse_address(&address, "127.0.0.1", "8988");

    pollev::PollEventGenerator generator;
    servev::ServerEventSource serverSource {address, 1, generator, [&](rwev::RWEventSource&& source) {
        std::shared_ptr<rwev::RWEventSource> sourcePtr {new rwev::RWEventSource {std::move(source)}};
        sourcePtr->setRWEvent(POLLIN);
        sourcePtr->addRWEventListener(RWEventClosure {sourcePtr});
    }, [&](int err) {
        std::cout << utils::err_message("accept", err) << std::endl;
    }};

    cliev::ClientEventSource clientSource {generator};

    utils::parse_address(&address, "nsu.ru", "80");
    std::cout << "parsed" << std::endl;

    clientSource.asyncConnect(address, [](rwev::RWEventSource&& source) {
        std::cout << "Success" << std::endl;
    }, [](int err) {
        std::cout << utils::err_message("connect", err) << std::endl;
    });

    for (;;) {
        generator.generateIOEvents();
    }

    return EXIT_SUCCESS;
}