#ifndef LAB31_POLL_EVENT_H
#define LAB31_POLL_EVENT_H

#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <stdexcept>
#include <algorithm>

#include <iostream>

#include <poll.h>
#include <unistd.h>

namespace pollev {
    using PollEventListener = std::function<void(short)>; //(revents)
    using RemoveListener = std::function<void()>;

    class PollList {
    public:
        class Index {
        friend PollList;
        private:
            bool added = false;
            size_t index;
        public:
            Index() {}
            Index(size_t index): index {index} {}
        };
    private:
        struct PollEventProviderData {
            std::vector<PollEventListener> listeners;
            std::vector<RemoveListener> removeListeners;
            std::shared_ptr<Index> index;
            bool removed = false;

            PollEventProviderData() {}
            PollEventProviderData(const std::shared_ptr<Index>& index): index {index} {}
            PollEventProviderData(const PollEventProviderData&)=delete;
            PollEventProviderData& operator=(const PollEventProviderData&)=delete;

            PollEventProviderData(PollEventProviderData&& other)=default;
            PollEventProviderData& operator=(PollEventProviderData&& other)=default;
        };

        std::vector<pollfd> pollfdList;
        std::vector<PollEventProviderData> providerDataList;

        std::vector<std::pair<pollfd, PollEventProviderData>> pollfdToAdd;
        size_t toRemoveCount = 0;

        void commitAdd();
        void commitRemove();

        void commit() {
            commitRemove();
            commitAdd();
        }

        bool applyTo(const Index& index, std::function<void(pollfd&, PollEventProviderData&)> f);

        bool applyTo(const Index& index, std::function<void(pollfd&)> f) {
            return applyTo(index, [&](pollfd& pfd, PollEventProviderData& data) {
                f(pfd);
            });
        }

        bool applyTo(const Index& index, std::function<void(PollEventProviderData&)> f) {
            return applyTo(index, [&](pollfd& pfd, PollEventProviderData& data) {
                f(data);
            });
        }

    public:
        void add(const pollfd& pfd, const std::shared_ptr<Index>& index) {
            index->index = pollfdToAdd.size();
            pollfdToAdd.push_back(std::make_pair(pfd, PollEventProviderData {index}));
        }

        bool remove(const Index& index) {
            if (index.added) {
                if (index.index > size()) return false;
                toRemoveCount++;
                providerDataList[index.index].removed = true;
            } else {
                if (index.index > pollfdToAdd.size()) return false;
                const size_t lastIndex = pollfdToAdd.size() - 1;
                std::swap(pollfdToAdd[index.index], pollfdToAdd[lastIndex]);
                std::swap(pollfdToAdd[index.index].second.index, pollfdToAdd[lastIndex].second.index);
                pollfdToAdd.resize(lastIndex);
            }

            return true;
        }

        size_t size() {return pollfdList.size();}

        //Returns false if interrupted
        bool poll();

        bool addListener(const Index& index, const PollEventListener& listener) {
            return applyTo(index, [&](PollEventProviderData& data) {
                data.listeners.push_back(listener);
            });
        }

        bool addRemoveListener(const Index& index, const RemoveListener& listener) {
            return applyTo(index, [&](PollEventProviderData& data) {
                data.removeListeners.push_back(listener);
            });
        }

        bool setEvent(const Index& index, short event) {
            return applyTo(index, [=] (pollfd& pfd) {
                pfd.events |= event;
            });
        }

        bool unsetEvent(const Index& index, short event) {
            return applyTo(index, [=] (pollfd& pfd) {
                pfd.events &= ~event;
            });
        }
    };

    class PollEventProvider {
    private:
        std::shared_ptr<PollList> pollList;
        std::shared_ptr<PollList::Index> index;
    public:
        PollEventProvider() {}
        PollEventProvider(const std::shared_ptr<PollList>& pollList, const std::shared_ptr<PollList::Index>& index): pollList {pollList}, index {index} {}
        ~PollEventProvider() {
            if (index.get() != NULL && pollList.get() != NULL) {
                pollList->remove(*index);
            }
        }

        PollEventProvider(const PollEventProvider&) = delete;
        PollEventProvider& operator=(const PollEventProvider&) = delete;

        PollEventProvider(PollEventProvider&& other):
            pollList {std::move(other.pollList)},
            index {std::move(other.index)} {}

        PollEventProvider& operator=(PollEventProvider&& other) {
            if (&other == this) return *this;

            pollList = std::move(other.pollList);
            index = std::move(other.index);

            return *this;
        }

        //PollEventProvider owns all listeners in a sense that listeners are destroyed if and only if PollEventProvider is destroyed
        //With only exception that the listener that caused destruction of owning Provider is executed completely and is the last to be executed for this Provider
        //If listener makes std::shared_ptr references to PollEventProvider, it may cause dangling references if not reset()
        void addListener(const PollEventListener& listener) {
            pollList->addListener(*index, listener);
        }

        void addRemoveListener(const RemoveListener& listener) {
            pollList->addRemoveListener(*index, listener);
        }

        void setEvent(short event) {
            pollList->setEvent(*index, event);
        }

        void unsetEvent(short event) {
            pollList->unsetEvent(*index, event);
        }
    };

    class PollEventGenerator {
    private:
        std::shared_ptr<PollList> pollList;
        std::shared_ptr<std::unordered_set<int>> fdsInList;
    public:
        PollEventGenerator() {
            pollList = std::make_shared<PollList>();
            fdsInList = std::make_shared<std::unordered_set<int>>();
        }

        //Returns false if interrupted
        bool generateIOEvents() {
            return pollList->poll();
        }

        //Throws if fd is invalid or has already been added
        PollEventProvider add(int fd, short events);
    };
}

#endif // !LAB31_POLL_EVENT_H