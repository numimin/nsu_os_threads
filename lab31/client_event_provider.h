#ifndef LAB31_CLIENT_EVENT_PROVIDER_H
#define LAB31_CLIENT_EVENT_PROVIDER_H

#include "rw_event_provider.h"
#include "utils.h"

#include <fcntl.h>

using namespace rwev;
using namespace utils;

namespace cliev {

    class ClientEventSource {
    private:
        RWEventGenerator generator;
        std::vector<IndexedData<RWEventSource>> connectionsInProgress;
    public:
        ClientEventSource(const PollEventGenerator& generator): 
            generator {generator} {}

        void asyncConnect(const SocketAddress& address, const RWProviderAddedListener& callback, const ErrorListener& errorHandler) {
            const int sockfd = socket(address.address.sa_family, SOCK_STREAM, 0);
            if (sockfd == -1) {
                const std::string message = err_message("connect");
                close(sockfd);
                throw std::runtime_error {message};
            }

            const int oldFlags = fcntl(sockfd, F_GETFL);
            if (oldFlags == -1) {
                const std::string message = err_message("fcntl");
                close(sockfd);
                throw std::runtime_error {message};
            }

            if (fcntl(sockfd, F_SETFL, oldFlags | O_NONBLOCK) == -1) {
                const std::string message = err_message("fcntl");
                close(sockfd);
                throw std::runtime_error {message};
            }

            if (connect(sockfd, &address.address, address.length)) {
                const int err = errno;

                if (err == EINPROGRESS) {
                    RWEventSource source = this->generator.add(sockfd, POLLOUT);
                    std::shared_ptr<int> index = std::make_shared<int>(connectionsInProgress.size());
                    connectionsInProgress.push_back(std::move(IndexedData {std::move(source), index}));

                    connectionsInProgress[*index].data.addRWEventListener([=](short event) {
                        if ((event & POLLOUT) != POLLOUT) return;

                        const size_t lastIndex = connectionsInProgress.size() - 1;
                        std::swap(connectionsInProgress[*index], connectionsInProgress[lastIndex]);
                        connectionsInProgress[lastIndex].data.unsetRWEvent(POLLOUT);

                        auto tmp = std::move(connectionsInProgress[lastIndex].data);
                        connectionsInProgress.resize(lastIndex);

                        if (fcntl(sockfd, F_SETFL, oldFlags) == -1) {
                            errorHandler(errno);
                            return;
                        }

                        callback(std::move(tmp));
                    });

                    connectionsInProgress[*index].data.addRemoveListener([=]() {
                        close(sockfd);
                    });

                    return;
                }

                errorHandler(err);
                const std::string message = err_message("connect");
                close(sockfd);
                return;
            }

            callback(this->generator.add(sockfd));
        }
    };
}

#endif // !LAB31_CLIENT_EVENT_PROVIDER_H