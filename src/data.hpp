#pragma once
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
            InitLoad();//数据初始化加载
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
            Storage();
            return true;
        }
        bool Update(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            _table[info.url_path]=info;
            pthread_rwlock_unlock(&_rwlock);
            Storage();
            return true;
        }
        bool GetOneByURL(const std::string &url,BackupInfo *info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto e = _table.find(url);
            if(e==_table.end())
            {
                //pthread_rwlock_destroy(&_rwlock);
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }
            *info=e->second;
            //pthread_rwlock_destroy(&_rwlock);
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
        bool GetOneByRealPath(const std::string &realpath,BackupInfo *info)
        {
            pthread_rwlock_wrlock(&_rwlock);
            auto e = _table.begin();
            for(;e!=_table.end();++e)
            {
                if(e->second.real_path==realpath)
                {
                    *info=e->second;
                    //pthread_rwlock_destroy(&_rwlock);
                    pthread_rwlock_unlock(&_rwlock);
                    return true;
                }
            }
            //pthread_rwlock_destroy(&_rwlock);
            pthread_rwlock_unlock(&_rwlock);
            return false;
        }
        bool GetAll(std::vector<BackupInfo> *arry)
        {
            //std::cout<<"call GetAll"<<std::endl;
            pthread_rwlock_wrlock(&_rwlock);
            auto e=_table.begin();
            for(;e!=_table.end();++e)
            {  
                arry->push_back(e->second);
            }
            pthread_rwlock_unlock(&_rwlock); //这里是解锁 不是把锁销毁
            return true;
        }
        //存储
        bool Storage()
        {
            //获取数据
            std::vector<BackupInfo> arry;
            this->GetAll(&arry);
            //存储到Json中
            Json::Value root;
            for(int i=0;i<arry.size();i++)
            {
                Json::Value tmp;
                tmp["pack_flag"]=arry[i].pack_flag;
                tmp["fsize"]=(Json::Int64)arry[i].fsize;//类型转换
                tmp["atime"]=(Json::Int64)arry[i].atime;
                tmp["mtime"]=(Json::Int64)arry[i].mtime;
                tmp["pack_path"]=arry[i].pack_path;
                tmp["real_path"]=arry[i].real_path;
                tmp["url_path"]=arry[i].url_path;
                root.append(tmp);
            }
            //序列化
            std::string body;
            JsonUtil::Serialize(root,&body);
            //重新写入文件
            FileUtil fu(_backup_file);
            fu.SetContent(body);
            return true;
        }
        //初始化加载,从配置文件中加载的
        bool InitLoad()
        {
            //获取数据文件中的数据
            FileUtil fu(_backup_file);
            if(fu.Exists()==false)//判断文件是否存在
            {
                return true;
            }
            std::string body;
            fu.GetContent(&body);
            //反序列化
            Json::Value root;
            JsonUtil::UnSerialize(body,&root);
            //将反序列化后的Json::Value数据添加到table中
            for(int i=0;i<root.size();i++)
            {
                BackupInfo info;
                info.pack_flag=root[i]["pack_flag"].asBool();
                info.fsize=root[i]["fsize"].asInt64();
                info.atime=root[i]["atime"].asInt64();
                info.mtime=root[i]["mtime"].asInt64();
                info.pack_path=root[i]["pack_path"].asString();
                info.real_path=root[i]["real_path"].asString();
                info.url_path=root[i]["url_path"].asString();
                Insert(info);
            }
            return true;
        }

    };
}
