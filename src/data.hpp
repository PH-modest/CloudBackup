#pragma once

#include "util.hpp"
#include "config.hpp"
#include <mysql/mysql.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>

namespace cloud
{
    typedef struct BackupInfo
    {
        bool pack_flag;
        size_t fsize;
        time_t atime;
        time_t mtime;

        // 为了兼容 service.hpp 中部分旧逻辑，字段仍然保留
        // 数据库存储后，real_path 不再是真实磁盘路径，而是保存文件名
        std::string real_path;
        std::string pack_path;
        std::string url_path;
        std::string uploader;
        std::string partition;
        int download_count;

        // 新增：数据库存储文件时使用
        std::string file_name;
        std::string file_data;

        bool NewBackupInfoForDB(const std::string &filename,
                                const std::string &content,
                                const std::string &user,
                                const std::string &part)
        {
            Config *config = Config::GetInstance();

            this->pack_flag = false;
            this->fsize = content.size();
            this->atime = time(nullptr);
            this->mtime = time(nullptr);

            this->file_name = filename;
            this->file_data = content;

            this->real_path = filename;
            this->pack_path = "";

            std::string user_dir = (part == "private"
                                        ? config->GetPrivateDirPrefix() + user + "/"
                                        : "public/");

            this->url_path = config->GetDownloadPrefix() + user_dir + filename;
            this->uploader = user;
            this->partition = part;
            this->download_count = 0;

            return true;
        }
    } BackupInfo;

    class DataManager
    {
    private:
        MYSQL *_mysql;
        pthread_rwlock_t _rwlock;

    private:
        bool Connect()
        {
            Config *conf = Config::GetInstance();

            _mysql = mysql_init(nullptr);
            if (_mysql == nullptr)
            {
                std::cerr << "mysql_init failed" << std::endl;
                return false;
            }

            mysql_options(_mysql, MYSQL_SET_CHARSET_NAME, "utf8mb4");

            if (mysql_real_connect(_mysql,
                                   conf->GetMysqlHost().c_str(),
                                   conf->GetMysqlUser().c_str(),
                                   conf->GetMysqlPass().c_str(),
                                   conf->GetMysqlDB().c_str(),
                                   conf->GetMysqlPort(),
                                   nullptr,
                                   0) == nullptr)
            {
                std::cerr << "mysql_real_connect failed: "
                          << mysql_error(_mysql) << std::endl;
                mysql_close(_mysql);
                _mysql = nullptr;
                return false;
            }

            return true;
        }

        bool ExecSQL(const std::string &sql)
        {
            if (_mysql == nullptr)
            {
                std::cerr << "mysql connection is null" << std::endl;
                return false;
            }

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "mysql_query failed: "
                          << mysql_error(_mysql)
                          << "\nSQL: " << sql << std::endl;
                return false;
            }

            return true;
        }

        std::string Escape(const std::string &src)
        {
            if (_mysql == nullptr || src.empty())
            {
                return "";
            }

            std::string dst;
            dst.resize(src.size() * 2 + 1);

            unsigned long len = mysql_real_escape_string(
                _mysql,
                &dst[0],
                src.data(),
                src.size());

            dst.resize(len);
            return dst;
        }

    public:
        DataManager()
            : _mysql(nullptr)
        {
            pthread_rwlock_init(&_rwlock, nullptr);

            if (!Connect())
            {
                std::cerr << "connect mysql failed!" << std::endl;
            }
        }

        ~DataManager()
        {
            if (_mysql)
            {
                mysql_close(_mysql);
                _mysql = nullptr;
            }

            pthread_rwlock_destroy(&_rwlock);
        }

        bool Insert(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);

            std::string sql =
                "INSERT INTO cloud_files "
                "(url_path, file_name, uploader, file_partition, fsize, atime, mtime, download_count, file_data) "
                "VALUES ('" +
                Escape(info.url_path) + "', '" +
                Escape(info.file_name) + "', '" +
                Escape(info.uploader) + "', '" +
                Escape(info.partition) + "', " +
                std::to_string(info.fsize) + ", " +
                std::to_string(info.atime) + ", " +
                std::to_string(info.mtime) + ", " +
                std::to_string(info.download_count) + ", '" +
                Escape(info.file_data) + "')";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool Delete(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);

            std::string sql =
                "DELETE FROM cloud_files WHERE url_path='" +
                Escape(info.url_path) + "'";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool Update(const BackupInfo &info)
        {
            pthread_rwlock_wrlock(&_rwlock);

            std::string sql =
                "UPDATE cloud_files SET "
                "atime=" + std::to_string(info.atime) +
                ", mtime=" + std::to_string(info.mtime) +
                ", download_count=" + std::to_string(info.download_count) +
                " WHERE url_path='" + Escape(info.url_path) + "'";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool GetOneByURL(const std::string &url, BackupInfo *info)
        {
            pthread_rwlock_rdlock(&_rwlock);

            std::string sql =
                "SELECT url_path, file_name, uploader, file_partition, fsize, atime, mtime, download_count, file_data "
                "FROM cloud_files WHERE url_path='" +
                Escape(url) + "' LIMIT 1";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "GetOneByURL failed: " << mysql_error(_mysql) << std::endl;
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            MYSQL_RES *res = mysql_store_result(_mysql);
            if (res == nullptr)
            {
                std::cerr << "mysql_store_result failed: " << mysql_error(_mysql) << std::endl;
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            MYSQL_ROW row = mysql_fetch_row(res);
            unsigned long *lengths = mysql_fetch_lengths(res);

            if (row == nullptr)
            {
                mysql_free_result(res);
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            info->url_path = row[0] ? row[0] : "";
            info->file_name = row[1] ? row[1] : "";
            info->uploader = row[2] ? row[2] : "";
            info->partition = row[3] ? row[3] : "";
            info->fsize = row[4] ? std::stoull(row[4]) : 0;
            info->atime = row[5] ? std::stoll(row[5]) : 0;
            info->mtime = row[6] ? std::stoll(row[6]) : 0;
            info->download_count = row[7] ? std::stoi(row[7]) : 0;

            if (row[8] != nullptr)
            {
                info->file_data.assign(row[8], lengths[8]);
            }
            else
            {
                info->file_data.clear();
            }

            info->pack_flag = false;
            info->pack_path = "";
            info->real_path = info->file_name;

            mysql_free_result(res);
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool GetOneByRealPath(const std::string &realpath, BackupInfo *info)
        {
            // 数据库存储后不再根据真实磁盘路径查询。
            return false;
        }

        bool GetAll(std::vector<BackupInfo> *arry)
        {
            pthread_rwlock_rdlock(&_rwlock);

            std::string sql =
                "SELECT url_path, file_name, uploader, file_partition, fsize, atime, mtime, download_count "
                "FROM cloud_files";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "GetAll failed: " << mysql_error(_mysql) << std::endl;
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            MYSQL_RES *res = mysql_store_result(_mysql);
            if (res == nullptr)
            {
                std::cerr << "mysql_store_result failed: " << mysql_error(_mysql) << std::endl;
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                BackupInfo info;
                info.url_path = row[0] ? row[0] : "";
                info.file_name = row[1] ? row[1] : "";
                info.uploader = row[2] ? row[2] : "";
                info.partition = row[3] ? row[3] : "";
                info.fsize = row[4] ? std::stoull(row[4]) : 0;
                info.atime = row[5] ? std::stoll(row[5]) : 0;
                info.mtime = row[6] ? std::stoll(row[6]) : 0;
                info.download_count = row[7] ? std::stoi(row[7]) : 0;

                info.pack_flag = false;
                info.real_path = info.file_name;
                info.pack_path = "";
                info.file_data = "";

                arry->push_back(info);
            }

            mysql_free_result(res);
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }
    };
}