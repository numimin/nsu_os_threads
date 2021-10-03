#ifndef LAB31_SERVER_EVENT_PROVIDER
#define LAB31_SERVER_EVENT_PROVIDER

#include "rw_event_provider.h"
#include "utils.h"

#include <string>

using namespace rwev;
using namespace utils;

namespace servev {
    class ServerEventSource {
    private:
        RWEventGenerator generator;
        RWEventSource acceptSource;
    public:
        ServerEventSource(const SocketAddress& address, int backlog, const PollEventGenerator& generator, const RWProviderAddedListener& addedListener, const ErrorListener& errorListener): 
            generator {generator}
        {
            const int acceptFd = server_setup(address, backlog);

            acceptSource = RWEventSource {this->generator.add(acceptFd, POLLIN)};

            acceptSource.addRemoveListener([this, acceptFd]() {
                close(acceptFd);
                this->resume(); //In case we've paused because fd limit in a process/system has been reached
            });

            acceptSource.addRWEventListener([=] (short event) {
                if ((event & POLLIN) != POLLIN) return;

                sockaddr_in acceptAddress;
                socklen_t addrLen = sizeof(address);

                int clientFd = accept(acceptFd, (sockaddr*) &acceptAddress, &addrLen);
                if (clientFd == -1) {
                    const int err = errno;
                    if (err == ENFILE || err == EMFILE) {
                        this->pause();
                    }
                    errorListener(errno);
                    return; //probably should add error handlers on top of that;
                }

                addedListener(this->generator.add(clientFd));
            });
        }

        void pause() {
            acceptSource.unsetRWEvent(POLLIN);
        }

        void resume() {
            acceptSource.setRWEvent(POLLIN);
        }
    };
}

#endif // !LAB31_SERVER_EVENT_PROVIDER