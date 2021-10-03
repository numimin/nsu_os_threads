#ifndef LAB31_PROXY_H
#define LAB31_PROXY_H

#include <vector>
#include <poll.h>
#include <cstddef>

struct SocketAddress {

};

constexpr size_t listener_index = 0;

class Proxy {
private:
    std::vector<pollfd> connections;
    int listener_fd() {connections[listener_index].fd;}
public:
    Proxy(const SocketAddress& bind_address);
    int run();
};

#endif // !LAB31_PROXY_H