#pragma once
#include "util.hpp"
#include <unordered_map>
#include <pthread.h>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace cloud
{
#define USERS_FILE "./users.dat"

    class UserManager
    {
    private:
        std::string _users_file;
        std::unordered_map<std::string, std::string> _users;
        pthread_rwlock_t _rwlock;

        std::atomic<bool> _need_storage{false};
        std::thread _storage_thread;
        std::condition_variable _cv;
        std::mutex _cv_mutex;

    public:
        UserManager()
            : _users_file(USERS_FILE)
        {
            pthread_rwlock_init(&_rwlock, NULL);
            InitLoad();

            _storage_thread = std::thread([this]() {
                while (true) {
                    std::unique_lock<std::mutex> lock(_cv_mutex);
                    _cv.wait_for(lock, std::chrono::seconds(1), [this]() { return _need_storage.load(); });

                    if (_need_storage.load()) {
                        pthread_rwlock_rdlock(&_rwlock);
                        Json::Value root;
                        for (const auto &p : _users) {
                            Json::Value user;
                            user["username"] = p.first;
                            user["password"] = p.second;
                            root.append(user);
                        }
                        pthread_rwlock_unlock(&_rwlock);

                        std::string body;
                        JsonUtil::Serialize(root, &body);
                        FileUtil fu(_users_file);
                        fu.SetContent(body);

                        _need_storage = false;
                    }
                }
            });
            _storage_thread.detach();
        }

        ~UserManager()
        {
            pthread_rwlock_destroy(&_rwlock);
        }

        bool UserExists(const std::string &user)
        {
            pthread_rwlock_rdlock(&_rwlock);
            bool exists = _users.count(user) > 0;
            pthread_rwlock_unlock(&_rwlock);
            return exists;
        }

        bool GetPassword(const std::string &user, std::string *pass)
        {
            pthread_rwlock_rdlock(&_rwlock);
            auto it = _users.find(user);
            if (it == _users.end()) {
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }
            *pass = it->second;
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        void AddUser(const std::string &user, const std::string &pass)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _users[user] = pass;
            _need_storage = true;
            _cv.notify_one();
            pthread_rwlock_unlock(&_rwlock);
        }

        void UpdateUsername(const std::string &old_user, const std::string &new_user)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto it = _users.find(old_user);
            if (it != _users.end()) {
                std::string pass = it->second;
                _users.erase(it);
                _users[new_user] = pass;
                _need_storage = true;
                _cv.notify_one();
            }
            pthread_rwlock_unlock(&_rwlock);
        }

        void UpdatePassword(const std::string &user, const std::string &new_pass)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _users[user] = new_pass;
            _need_storage = true;
            _cv.notify_one();
            pthread_rwlock_unlock(&_rwlock);
        }

        bool InitLoad()
        {
            FileUtil fu(_users_file);
            if (!fu.Exists()) {
                // 初始化 admin
                AddUser("admin", "password");
                return true;
            }
            std::string body;
            fu.GetContent(&body);
            Json::Value root;
            if (!JsonUtil::UnSerialize(body, &root)) {
                return false;
            }
            for (int i = 0; i < root.size(); ++i) {
                std::string user = root[i]["username"].asString();
                std::string pass = root[i]["password"].asString();
                _users[user] = pass;
            }
            return true;
        }
    };
}