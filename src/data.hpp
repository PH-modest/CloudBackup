#pragma once
#include "util.hpp"
#include "config.hpp"
#include <unordered_map>
#include <pthread.h>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace cloud
{
    typedef struct BackupInfo
    {
        bool pack_flag;
        size_t fsize;
        time_t atime;
        time_t mtime;
        std::string real_path;
        std::string pack_path;
        std::string url_path;
        std::string uploader;
        std::string partition;
        int download_count;  // 修改：添加下载次数字段

        bool NewBackupInfo(const std::string &realpath, const std::string &user, const std::string &part)
        {
            FileUtil fu(realpath);
            if (fu.Exists() == false)
            {
                std::cout << "new backupinfo: file not exists!\n";
                return false;
            }
            Config *config = Config::GetInstance();
            this->pack_flag = false;
            this->fsize = fu.FileSize();
            this->atime = fu.LastATime();
            this->mtime = fu.LastMTime();
            this->real_path = realpath;
            std::string user_dir = (part == "private" ? config->GetPrivateDirPrefix() + user + "/" : part + "/");
            this->pack_path = config->GetPackDir() + user_dir + fu.FileName() + config->GetPackFileSuffix();
            this->url_path = config->GetDownloadPrefix() + user_dir + fu.FileName();
            this->uploader = user;
            this->partition = part;
            this->download_count = 0;  // 修改：初始化下载次数为0
            return true;
        }
    } BackupInfo;

    class DataManager
    {
    private:
        std::string _backup_file;
        std::unordered_map<std::string, BackupInfo> _table;
        pthread_rwlock_t _rwlock;

        std::atomic<bool> _need_storage{false}; // 标记是否需要持久化
        std::thread _storage_thread;            // 后台持久化线程
        std::condition_variable _cv;            // 条件变量，触发持久化
        std::mutex _cv_mutex;

    private:
        void ScanAndSyncFolders()
        {
            Config *conf = Config::GetInstance();
            std::string back_dir = conf->GetBackDir();
            std::string pack_dir = conf->GetPackDir();
            std::string pack_suffix = conf->GetPackFileSuffix();
            std::string private_prefix = conf->GetPrivateDirPrefix();
            std::string download_prefix = conf->GetDownloadPrefix();

            // 辅助函数：检查文件是否已在_table中（基于url_path）
            auto is_in_table = [this](const std::string &url) -> bool
            {
                pthread_rwlock_rdlock(&_rwlock);
                bool exists = (_table.find(url) != _table.end());
                pthread_rwlock_unlock(&_rwlock);
                return exists;
            };

            // 扫描公共区 (backdir/public/ 和 packdir/public/)
            ScanDirAndAdd(back_dir + "public/", "public", false, pack_suffix, download_prefix, is_in_table);
            ScanDirAndAdd(pack_dir + "public/", "public", true, pack_suffix, download_prefix, is_in_table);

            // 扫描私有区 (backdir/private_user/* 和 packdir/private_user/*)
            for (auto &p : fs::directory_iterator(back_dir + private_prefix))
            {
                if (fs::is_directory(p))
                {
                    std::string user_dir = fs::path(p).filename().string() + "/";
                    std::string partition = "private";
                    std::string user = fs::path(p).filename().string(); // 用户名从目录名提取
                    ScanDirAndAdd(back_dir + private_prefix + user_dir, partition, false, pack_suffix, download_prefix, is_in_table, user);
                    ScanDirAndAdd(pack_dir + private_prefix + user_dir, partition, true, pack_suffix, download_prefix, is_in_table, user);
                }
            }

            // 如果扫描到新文件，标记需要存储
            _need_storage = true;
            _cv.notify_one();
        }

        void ScanDirAndAdd(const std::string &dir_path, const std::string &partition, bool is_pack, const std::string &pack_suffix,
                           const std::string &download_prefix, std::function<bool(const std::string &)> is_in_table,
                           const std::string &default_uploader = "unknown")
        {
            if (!fs::exists(dir_path))
                return;

            Config *conf = Config::GetInstance();
            std::string private_prefix = conf->GetPrivateDirPrefix();
            std::string back_dir = conf->GetBackDir();
            std::string pack_dir = conf->GetPackDir();

            for (auto &p : fs::directory_iterator(dir_path))
            {
                if (fs::is_directory(p))
                    continue; // 跳过子目录

                std::string full_path = fs::path(p).string();
                std::string filename = fs::path(p).filename().string();
                std::string url_path;

                if (is_pack)
                {
                    // packdir中的文件：去掉.lz后缀，计算url_path
                    if (filename.size() <= pack_suffix.size() || filename.substr(filename.size() - pack_suffix.size()) != pack_suffix)
                        continue;
                    std::string orig_filename = filename.substr(0, filename.size() - pack_suffix.size());
                    url_path = download_prefix + (partition == "public" ? "public/" : private_prefix + default_uploader + "/") + orig_filename;
                }
                else
                {
                    // backdir中的文件：直接计算url_path
                    url_path = download_prefix + (partition == "public" ? "public/" : private_prefix + default_uploader + "/") + filename;
                }

                if (is_in_table(url_path))
                    continue; // 已存在，跳过

                BackupInfo info;
                info.pack_flag = is_pack;
                FileUtil fu(full_path);
                info.fsize = fu.FileSize();
                info.atime = fu.LastATime();
                info.mtime = fu.LastMTime();
                info.real_path = is_pack ? dir_path.substr(0, dir_path.find("pack/")) + "back/" + filename.substr(0, filename.size() - pack_suffix.size()) : full_path;  // 修正 real_path
                // 修改：创建 dir_path 副本进行 replace 操作，避免 const 问题
                std::string modified_dir = dir_path;
                size_t pos = modified_dir.find("back/");
                if (pos != std::string::npos) {
                    modified_dir.replace(pos, 5, "pack/");
                } else {
                    // 如果未找到 "back/"，使用原 dir_path（或根据实际路径调整，假设配置正确）
                    std::cerr << "Warning: 'back/' not found in dir_path: " << dir_path << std::endl;
                }
                info.pack_path = is_pack ? full_path : modified_dir + filename + pack_suffix;
                info.url_path = url_path;
                info.uploader = default_uploader;
                info.partition = partition;
                info.download_count = 0;  // 修改：扫描添加时初始化下载次数为0

                Insert(info);
            }
        }

    public:
        DataManager()
        {
            Config *conf = Config::GetInstance();
            _backup_file = conf->GetBackupFile();
            pthread_rwlock_init(&_rwlock, nullptr);
            InitLoad();
            _storage_thread = std::thread([this]() {
                while (true) {
                    std::unique_lock<std::mutex> lock(_cv_mutex);
                    _cv.wait(lock, [this]() { return _need_storage.load(); });
                    Storage();
                    _need_storage = false;
                }
            });
        }
        ~DataManager()
        {
            _need_storage = true;
            _cv.notify_all();
            _storage_thread.join();  // 作为最终保障，手动调用 Storage()
            Storage();
            pthread_rwlock_destroy(&_rwlock);
        }
        bool Insert(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _table[info.url_path] = info;
            _need_storage = true; // 标记需要持久化
            _cv.notify_one();     // 通知后台线程进行持久化
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool Delete(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto e = _table.find(info.url_path);
            if (e == _table.end())
            {
                pthread_rwlock_unlock(&_rwlock);
                std::cerr << "Delete failed: URL not found - " << info.url_path << std::endl; // 新增日志
                return false;
            }
            _table.erase(e);
            _need_storage = true;
            _cv.notify_one();
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool Update(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _table[info.url_path] = info;
            _need_storage = true; // 标记需要持久化
            _cv.notify_one();     // 通知后台线程进行持久化
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool GetOneByURL(const std::string &url, BackupInfo *info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto e = _table.find(url);
            if (e == _table.end())
            {
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }
            *info = e->second;
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool GetOneByRealPath(const std::string &realpath, BackupInfo *info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto e = _table.begin();
            for (; e != _table.end(); ++e)
            {
                if (e->second.real_path == realpath)
                {
                    *info = e->second;
                    pthread_rwlock_unlock(&_rwlock);
                    return true;
                }
            }
            pthread_rwlock_unlock(&_rwlock);
            return false;
        }
        bool GetAll(std::vector<BackupInfo> *arry)
        {
            pthread_rwlock_wrlock(&_rwlock);
            for (auto &p : _table)
            {
                arry->push_back(p.second);
            }
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        // 存储
        bool Storage()
        {
            // 获取数据
            std::vector<BackupInfo> arry;
            this->GetAll(&arry);
            // 存储到Json中
            Json::Value root;
            for (int i = 0; i < arry.size(); i++)
            {
                Json::Value tmp;
                tmp["pack_flag"] = arry[i].pack_flag;
                tmp["fsize"] = (Json::Int64)arry[i].fsize; // 类型转换
                tmp["atime"] = (Json::Int64)arry[i].atime;
                tmp["mtime"] = (Json::Int64)arry[i].mtime;
                tmp["pack_path"] = arry[i].pack_path;
                tmp["real_path"] = arry[i].real_path;
                tmp["url_path"] = arry[i].url_path;
                tmp["uploader"] = arry[i].uploader;
                tmp["partition"] = arry[i].partition;
                tmp["download_count"] = arry[i].download_count;  // 修改：序列化下载次数
                root.append(tmp);
            }
            // 序列化
            std::string body;
            JsonUtil::Serialize(root, &body);
            // 重新写入文件
            FileUtil fu(_backup_file);
            fu.SetContent(body);
            // std::cout << "Manual storage completed." << std::endl;  // 可选日志
            return true;
        }
        // 初始化加载,从配置文件中加载的
        bool InitLoad()
        {
            // 先加载cloud.dat（原有逻辑）
            FileUtil fu(_backup_file);
            if (fu.Exists())
            {
                std::string body;
                fu.GetContent(&body);
                Json::Value root;
                if (JsonUtil::UnSerialize(body, &root))
                {
                    for (int i = 0; i < root.size(); i++)
                    {
                        BackupInfo info;
                        info.pack_flag = root[i]["pack_flag"].asBool();
                        info.fsize = root[i]["fsize"].asInt64();
                        info.atime = root[i]["atime"].asInt64();
                        info.mtime = root[i]["mtime"].asInt64();
                        info.pack_path = root[i]["pack_path"].asString();
                        info.real_path = root[i]["real_path"].asString();
                        info.url_path = root[i]["url_path"].asString();
                        info.uploader = root[i]["uploader"].asString();
                        info.partition = root[i]["partition"].asString();
                        info.download_count = root[i]["download_count"].asInt();  // 修改：反序列化下载次数（默认0如果不存在）
                        Insert(info); // 注意：Insert会设置_need_storage，但我们稍后会统一存储
                    }
                }
                else
                {
                    std::cout << "Failed to parse cloud.dat, scanning folders instead." << std::endl;
                }
            }
            else
            {
                std::cout << "cloud.dat not found, scanning folders." << std::endl;
            }

            // 扫描backdir和packdir，添加缺失的文件
            ScanAndSyncFolders();

            // 加载后立即持久化，确保_table与cloud.dat同步
            Storage();
            return true;
        }
    };
}