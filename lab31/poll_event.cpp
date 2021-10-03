#include "poll_event.h"

void pollev::PollList::commitAdd() {
    for (auto& p : pollfdToAdd) {
        p.second.index->added = true;
        p.second.index->index = size();
        pollfdList.push_back(p.first);
        providerDataList.push_back(std::move(p.second));
    }

    pollfdToAdd.clear();
}

void pollev::PollList::commitRemove() {
    if (toRemoveCount == 0) return;

    size_t j = size() - 1;
    const size_t newSize = size() - toRemoveCount;

    for (size_t i = 0; (i < j) && (toRemoveCount > 0); ++i) {
        while (providerDataList[j].removed && j > i) {
            j--;
            toRemoveCount--;
        }

        if (j == i) break;

        if (providerDataList[i].removed) {
            std::swap(providerDataList[i], providerDataList[j]);
            std::swap(pollfdList[i], pollfdList[j]);
            std::swap(providerDataList[i].index, providerDataList[j].index);
            j--;
            toRemoveCount--;
        }
    }

    providerDataList.resize(newSize);
    pollfdList.resize(newSize);
}

bool pollev::PollList::applyTo(const Index& index, std::function<void(pollfd&, PollEventProviderData&)> f) {
    if (index.added) {
        if (index.index > size()) return false;
        f(pollfdList[index.index], providerDataList[index.index]);
    } else {
        if (index.index > pollfdToAdd.size()) return false;
        f(pollfdToAdd[index.index].first, pollfdToAdd[index.index].second);
    }

    return true;
}

//Returns false if interrupted
bool pollev::PollList::poll() {
    commit();

    std::cout << "poll: " << size() << std::endl;

    if (size() == 0) return true;
    
    int count = ::poll(pollfdList.data(), size(), -1);
    //can actually be safe from all errors but EAGAIN/EINTR (for EINVAL we must ensure that we don't store same fds in different handles)
    //Safety from EINVAL is guaranteed if accessed only through PollEventGenerator;
    if (count == -1) {
        int err = errno;
        if (err == EINTR || err == EAGAIN) {
            return false;
        }

        throw std::runtime_error {"poll"};
    }

    for (size_t i = 0; (count != 0) && (i < size()); ++i) {
        const short revents = pollfdList[i].revents;
        if (revents != 0) {
            for (const auto& l : providerDataList[i].listeners) {
                if (providerDataList[i].removed) break;
                l(revents);
            }
            count--;
        }
    }

    commit();
    return true;
}

//Throws if fd is invalid or has already been added
pollev::PollEventProvider pollev::PollEventGenerator::add(int fd, short events) {
    if (fd > sysconf(_SC_OPEN_MAX)) {
        throw std::runtime_error {"Fd is > OPEN_MAX"};
    }

    if (fdsInList->find(fd) != fdsInList->end()) {
        throw std::runtime_error {"fd has already been added"};
    }

    auto index = std::make_shared<PollList::Index>();
    PollEventProvider source {pollList, index};
    pollList->add(pollfd {.fd=fd, .events=events}, index);

    fdsInList->insert(fd);
    pollList->addRemoveListener(*index, [this, fd]() {
        fdsInList->erase(fd);
    });

    return source;
}
