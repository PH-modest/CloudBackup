#ifndef __MY_DATA__
#define __MY_DATA__
#include "util.hpp"
#include "config.hpp"
#include<unordered_map>
#include<pthread.h>

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
        bool NewBackupInfo(const std::string &realpath)
        {
            FileUtil fu(realpath);
            if(fu.Exists() == false)
            {
                std::cout<<"new backupinfo: file not exists!\n";
                return false;
            }
            //错误的定义
            // Config *config;
            // config->GetInstance();
            Config *config=Config::GetInstance();
            this->pack_flag=false;
            this->fsize=fu.FileSize();
            this->atime=fu.LastATime();
            this->mtime=fu.LastMTime();
            this->real_path=realpath;
            //./backdir/a.txt  ->  ./packdir/a.txt.lz
            this->pack_path=config->GetPackDir()+fu.FileName()+config->GetPackFileSuffix();
            //./backdir/a.txt  ->  /download/a.txt.lz
            this->url_path=config->GetDownloadPrefix()+fu.FileName();
            return true;
        }
    }BackupInfo;

    class DataManager
    {
    private:
        std::string _backup_file;
        std::unordered_map<std::string,BackupInfo> _table;
        pthread_rwlock_t _rwlock;
    public:
        DataManager()
        {
            _backup_file=Config::GetInstance()->GetBackupFile();
            pthread_rwlock_init(&_rwlock,NULL);
        }
        ~DataManager()
        {
            pthread_rwlock_destroy(&_rwlock);
        }
        bool Insert(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _table[info.url_path]=info;
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool Update(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _table[info.url_path]=info;
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool GetOneByURL(const std::string &url,BackupInfo *info);
        bool GetOneByRealPath(const std::string &realpath,BackupInfo *info);
        bool GetAll(std::vector<BackupInfo> *arry);
    };
}

#endif