#pragma once

#include "util.hpp"
#include "config.hpp"
#include <mysql/mysql.h>
#include <pthread.h>
#include <string>
#include <iostream>
#include <ctime>

namespace cloud
{
    class UserManager
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
                std::cerr << "mysql_init failed in UserManager" << std::endl;
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
                std::cerr << "mysql_real_connect failed in UserManager: "
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
                std::cerr << "mysql connection is null in UserManager" << std::endl;
                return false;
            }

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "mysql_query failed in UserManager: "
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

        void InitDefaultAdmin()
        {
            if (UserExists("admin"))
            {
                return;
            }

            time_t now = time(nullptr);

            std::string sql =
                "INSERT INTO cloud_users "
                "(username, password, user_role, created_at, updated_at) "
                "VALUES ('admin', 'password', 'admin', " +
                std::to_string(now) + ", " + std::to_string(now) + ")";

            ExecSQL(sql);
        }

    public:
        UserManager()
            : _mysql(nullptr)
        {
            pthread_rwlock_init(&_rwlock, nullptr);

            if (!Connect())
            {
                std::cerr << "connect mysql failed in UserManager!" << std::endl;
                return;
            }

            InitDefaultAdmin();
        }

        ~UserManager()
        {
            if (_mysql)
            {
                mysql_close(_mysql);
                _mysql = nullptr;
            }

            pthread_rwlock_destroy(&_rwlock);
        }

        bool UserExists(const std::string &user)
        {
            pthread_rwlock_rdlock(&_rwlock);

            std::string sql =
                "SELECT id FROM cloud_users WHERE username='" +
                Escape(user) + "' LIMIT 1";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "UserExists failed: " << mysql_error(_mysql) << std::endl;
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
            bool exists = (row != nullptr);

            mysql_free_result(res);
            pthread_rwlock_unlock(&_rwlock);
            return exists;
        }

        bool GetPassword(const std::string &user, std::string *pass)
        {
            pthread_rwlock_rdlock(&_rwlock);

            std::string sql =
                "SELECT password FROM cloud_users WHERE username='" +
                Escape(user) + "' LIMIT 1";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "GetPassword failed: " << mysql_error(_mysql) << std::endl;
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
            if (row == nullptr)
            {
                mysql_free_result(res);
                pthread_rwlock_unlock(&_rwlock);
                return false;
            }

            *pass = row[0] ? row[0] : "";

            mysql_free_result(res);
            pthread_rwlock_unlock(&_rwlock);
            return true;
        }

        bool AddUser(const std::string &user, const std::string &pass)
        {
            pthread_rwlock_wrlock(&_rwlock);

            time_t now = time(nullptr);

            std::string sql =
                "INSERT INTO cloud_users "
                "(username, password, user_role, created_at, updated_at) "
                "VALUES ('" +
                Escape(user) + "', '" +
                Escape(pass) + "', 'user', " +
                std::to_string(now) + ", " +
                std::to_string(now) + ")";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool UpdateUsername(const std::string &old_user, const std::string &new_user)
        {
            pthread_rwlock_wrlock(&_rwlock);

            time_t now = time(nullptr);

            std::string sql =
                "UPDATE cloud_users SET username='" +
                Escape(new_user) +
                "', updated_at=" + std::to_string(now) +
                " WHERE username='" + Escape(old_user) + "'";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool UpdatePassword(const std::string &user, const std::string &new_pass)
        {
            pthread_rwlock_wrlock(&_rwlock);

            time_t now = time(nullptr);

            std::string sql =
                "UPDATE cloud_users SET password='" +
                Escape(new_pass) +
                "', updated_at=" + std::to_string(now) +
                " WHERE username='" + Escape(user) + "'";

            bool ret = ExecSQL(sql);

            pthread_rwlock_unlock(&_rwlock);
            return ret;
        }

        bool IsAdmin(const std::string &user)
        {
            pthread_rwlock_rdlock(&_rwlock);

            std::string sql =
                "SELECT user_role FROM cloud_users WHERE username='" +
                Escape(user) + "' LIMIT 1";

            if (mysql_query(_mysql, sql.c_str()) != 0)
            {
                std::cerr << "IsAdmin failed: " << mysql_error(_mysql) << std::endl;
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
            bool is_admin = false;

            if (row != nullptr && row[0] != nullptr)
            {
                is_admin = (std::string(row[0]) == "admin");
            }

            mysql_free_result(res);
            pthread_rwlock_unlock(&_rwlock);
            return is_admin;
        }
    };
}