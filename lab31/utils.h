#ifndef LAB31_UTILS_H
#define LAB31_UTILS_H

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cstring>

#include <stdexcept>
#include <iostream>

namespace utils {
    std::string err_message(const std::string& message, int err) {
        return message + ": " + std::string {strerror(err)};
    }

    std::string err_message(const std::string& message) {
        return err_message(message, errno);
    }

    typedef struct {
        struct sockaddr address;
        socklen_t length;
        sa_family_t family;
    } SocketAddress;

    void parse_address(SocketAddress* address, const std::string& addr_str, const std::string& port_str) {
        struct addrinfo* addrinfo;

        const int err_code = getaddrinfo(addr_str.c_str(), port_str.c_str(), NULL, &addrinfo);
        if (err_code != 0) {
            throw std::runtime_error {"getaddrinfo: " + std::string {gai_strerror(err_code)}};
        }

        memcpy(&address->address, addrinfo->ai_addr, addrinfo->ai_addrlen);
        address->length = addrinfo->ai_addrlen;
        address->family = addrinfo->ai_family;
        freeaddrinfo(addrinfo);
    }

    int server_setup(const SocketAddress& address, int backlog) {
        const int sockfd = socket(address.address.sa_family, SOCK_STREAM, 0);
        if (sockfd == -1) {
            throw std::runtime_error {err_message("socket")};
        }

        if (bind(sockfd, &address.address, address.length)) {
            close(sockfd);
            throw std::runtime_error {err_message("bind")};
        }

        if (listen(sockfd, backlog)) {
            close(sockfd);
            throw std::runtime_error {err_message("listen")};
        }

        return sockfd;
    }

    template <typename D>
    struct IndexedData {
        D data;
        std::shared_ptr<int> index;

        IndexedData()=default;
        ~IndexedData()=default;

        IndexedData(D& data, std::shared_ptr<int> index): data {data}, index {index} {}
        IndexedData(D&& data, std::shared_ptr<int> index): data {std::move(data)}, index {index} {}

        IndexedData(const IndexedData& other)=delete;
        IndexedData& operator=(const IndexedData& other)=delete;

        /*IndexedData(const IndexedData& other): data {other.data}, index {index} {}
        IndexedData& operator=(const IndexedData& other) {
            if (&other == this) return *this;

            data = other.data;
            index = other.index;

            return *this;
        }*/

        IndexedData(IndexedData&& other): data {std::move(other.data)}, index {std::move(index)} {}
        IndexedData& operator=(IndexedData&& other) {
            if (&other == this) return *this;

            data = std::move(other.data);
            index = std::move(other.index);

            return *this;
        }
    };

    /*int client_noblock_setup(const SocketAddress* address) {
        const int sockfd = socket(address->address.sa_family, SOCK_STREAM, 0);
        if (sockfd == -1) {
            close(sockfd);
            throw std::runtime_error {err_message("connect")};
        }

        if (connect(sockfd, &address->address, address->length)) {
            close(sockfd);
            throw std::runtime_error {err_message("connect")};
        }

        return sockfd;
    }*/
};

using namespace utils;

template <typename D>
void std::swap(IndexedData<D>& lhs, IndexedData<D>& rhs) {
    std::swap(lhs.data, rhs.data); //Doesn't swap index (obviously)
}

#endif // !LAB31_UTILS_H