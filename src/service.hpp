#pragma once
#include "data.hpp"
#include "httplib.h"
#include "user_manager.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <ctime>
#include <regex>
#include <experimental/filesystem>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif

extern cloud::DataManager *_data;
extern cloud::UserManager *_user_mgr;

namespace cloud
{
    namespace fs = std::experimental::filesystem;
    // 加载模板函数
    inline std::string LoadTemplate(const std::string &filename, const std::unordered_map<std::string, std::string> &placeholders)
    { // 内联
        // 尝试多个可能的路径，优先查找 templates/ 目录
        std::vector<std::string> possible_paths = {
            "templates/" + filename,        // templates 目录（优先）
            "./templates/" + filename,      // 当前目录下的 templates
            filename,                       // 当前目录
            "./" + filename,                // 当前目录（显式）
            "../src/templates/" + filename, // 从上级目录的src/templates
            "src/templates/" + filename,    // src/templates子目录
        };

        std::string body;
        bool loaded = false;
        std::string used_path;
        for (const auto &path : possible_paths)
        {
            FileUtil fu(path);
            if (fu.GetContent(&body))
            {
                loaded = true;
                used_path = path;
                break;
            }
        }

        if (!loaded)
        {
            std::cerr << "LoadTemplate failed for: " << filename << " (tried multiple paths)" << std::endl;
            return "<h1>Error loading template: " + filename + "</h1>";
        }

        for (const auto &p : placeholders)
        {
            size_t pos = 0;
            std::string key = "{" + p.first + "}";
            while ((pos = body.find(key, pos)) != std::string::npos)
            {
                body.replace(pos, key.length(), p.second);
                pos += p.second.length();
            }
        }
        return body;
    }

    class Service
    {
    private:
        int _server_port;
        std::string _server_ip;
        std::string _download_prefix;
        httplib::Server _server;

        // 提取共享页面模板
        static const std::string page_template;

    private:
        static std::string EscapeHTML(const std::string &s)
        {
            std::string res;
            for (char c : s)
            {
                switch (c)
                {
                case '<':
                    res += "&lt;";
                    break;
                case '>':
                    res += "&gt;";
                    break;
                case '&':
                    res += "&amp;";
                    break;
                case '"':
                    res += "&quot;";
                    break;
                case '\'':
                    res += "&#39;";
                    break;
                default:
                    res += c;
                    break;
                }
            }
            return res;
        }

        static std::string EscapeJS(const std::string &s)
        {
            std::string res;
            for (char c : s)
            {
                if (c == '\'')
                    res += "\\'";
                else if (c == '\\')
                    res += "\\\\";
                else
                    res += c;
            }
            return res;
        }

        // 格式化时间戳为可读格式
        static std::string FormatTime(time_t timestamp)
        {
            if (timestamp <= 0)
                return "未知";

            struct tm *timeinfo = localtime(&timestamp);
            if (!timeinfo)
                return "未知";

            char buffer[80];
            strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
            return std::string(buffer);
        }

        // 分页辅助结构
        struct PaginationInfo
        {
            int current_page;
            int total_pages;
            int page_size;
            int total_items;
        };

        // 生成分页控件HTML
        static std::string GeneratePagination(const PaginationInfo &pagination, const std::string &base_url, const std::string &extra_params = "")
        {
            if (pagination.total_pages <= 1)
                return ""; // 只有一页或没有数据，不显示分页

            std::string pagination_html = "<div class=\"pagination\">";

            // 上一页按钮
            if (pagination.current_page > 1)
            {
                int prev_page = pagination.current_page - 1;
                pagination_html += "<a href=\"" + base_url + "?page=" + std::to_string(prev_page) + extra_params + "\" class=\"page-btn\">上一页</a>";
            }
            else
            {
                pagination_html += "<span class=\"page-btn disabled\">上一页</span>";
            }

            // 页码按钮
            int start_page = std::max(1, pagination.current_page - 2);
            int end_page = std::min(pagination.total_pages, pagination.current_page + 2);

            if (start_page > 1)
            {
                pagination_html += "<a href=\"" + base_url + "?page=1" + extra_params + "\" class=\"page-btn\">1</a>";
                if (start_page > 2)
                    pagination_html += "<span class=\"page-ellipsis\">...</span>";
            }

            for (int i = start_page; i <= end_page; i++)
            {
                if (i == pagination.current_page)
                {
                    pagination_html += "<span class=\"page-btn current\">" + std::to_string(i) + "</span>";
                }
                else
                {
                    pagination_html += "<a href=\"" + base_url + "?page=" + std::to_string(i) + extra_params + "\" class=\"page-btn\">" + std::to_string(i) + "</a>";
                }
            }

            if (end_page < pagination.total_pages)
            {
                if (end_page < pagination.total_pages - 1)
                    pagination_html += "<span class=\"page-ellipsis\">...</span>";
                pagination_html += "<a href=\"" + base_url + "?page=" + std::to_string(pagination.total_pages) + extra_params + "\" class=\"page-btn\">" + std::to_string(pagination.total_pages) + "</a>";
            }

            // 下一页按钮
            if (pagination.current_page < pagination.total_pages)
            {
                int next_page = pagination.current_page + 1;
                pagination_html += "<a href=\"" + base_url + "?page=" + std::to_string(next_page) + extra_params + "\" class=\"page-btn\">下一页</a>";
            }
            else
            {
                pagination_html += "<span class=\"page-btn disabled\">下一页</span>";
            }

            pagination_html += "<span class=\"page-info\">共 " + std::to_string(pagination.total_items) + " 个文件，第 " +
                               std::to_string(pagination.current_page) + " / " + std::to_string(pagination.total_pages) + " 页</span>";
            pagination_html += "</div>";

            return pagination_html;
        }

        static bool IsLoggedIn(const httplib::Request &req)
        {
            std::string user = GetUsername(req);
            return !user.empty() && _user_mgr->UserExists(user);
        }

        static std::string GetUsername(const httplib::Request &req)
        {
            if (req.has_header("Cookie"))
            {
                std::string cookie = req.get_header_value("Cookie");
                size_t start = cookie.find("username=");
                if (start != std::string::npos)
                {
                    start += 9;
                    size_t end = cookie.find(';', start);
                    if (end == std::string::npos)
                    {
                        end = cookie.length();
                    }
                    std::string user = cookie.substr(start, end - start);
                    user.erase(0, user.find_first_not_of(" \t"));
                    user.erase(user.find_last_not_of(" \t") + 1);
                    return user;
                }
            }
            return "";
        }

        static bool IsAdmin(const std::string &user)
        {
            return _user_mgr->IsAdmin(user);
        }

        static void Logout(const httplib::Request &req, httplib::Response &rsp)
        {
            rsp.set_header("Set-Cookie", "username=; Max-Age=0; Path=/");
            rsp.status = 302;
            rsp.set_header("Location", "/login");
        }

        static void Login(const httplib::Request &req, httplib::Response &rsp)
        {
            if (req.method == "GET")
            {
                std::unordered_map<std::string, std::string> placeholders;
                rsp.body = LoadTemplate("login.html", placeholders);
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                return;
            }

            if (req.method == "POST")
            {
                if (!req.has_param("username") || !req.has_param("password"))
                {
                    rsp.body = R"(<script>alert("用户名或密码不能为空！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 400;
                    return;
                }

                std::string user = req.get_param_value("username");
                std::string pass = req.get_param_value("password");

                if (user.empty() || pass.empty())
                {
                    rsp.body = R"(<script>alert("用户名或密码不能为空！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 400;
                    return;
                }

                std::string real_pass;
                if (!_user_mgr->GetPassword(user, &real_pass))
                {
                    rsp.body = R"(<script>alert("用户不存在，或数据库查询失败！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 401;
                    return;
                }

                if (real_pass != pass)
                {
                    rsp.body = R"(<script>alert("密码错误！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 401;
                    return;
                }

                // 注意：这里必须是 username，不是 user
                rsp.set_header("Set-Cookie", "username=" + user + "; Max-Age=3600; Path=/");
                rsp.status = 302;
                rsp.set_header("Location", "/main/");
                return;
            }

            rsp.status = 405;
            rsp.body = "Method Not Allowed";
        }

        static void Register(const httplib::Request &req, httplib::Response &rsp)
        {
            if (req.method == "GET")
            {
                std::unordered_map<std::string, std::string> placeholders;
                rsp.body = LoadTemplate("register.html", placeholders);
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                return;
            }

            if (req.method == "POST")
            {
                if (!req.has_param("username") || !req.has_param("password") || !req.has_param("confirm_password"))
                {
                    rsp.body = R"(<script>alert("注册参数不完整！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 400;
                    return;
                }

                std::string user = req.get_param_value("username");
                std::string pass = req.get_param_value("password");
                std::string confirm = req.get_param_value("confirm_password");

                if (user.empty() || pass.empty() || confirm.empty())
                {
                    rsp.body = R"(<script>alert("用户名或密码不能为空！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 400;
                    return;
                }

                if (pass != confirm)
                {
                    rsp.body = R"(<script>alert("两次输入的密码不一致！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 400;
                    return;
                }

                if (_user_mgr->UserExists(user))
                {
                    rsp.body = R"(<script>alert("用户已存在！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 409;
                    return;
                }

                if (!_user_mgr->AddUser(user, pass))
                {
                    rsp.body = R"(<script>alert("注册失败：数据库写入错误！"); history.back();</script>)";
                    rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                    rsp.status = 500;
                    return;
                }

                rsp.body = R"(<script>alert("注册成功，请登录！"); window.location='/login';</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 200;
                return;
            }

            rsp.status = 405;
            rsp.body = "Method Not Allowed";
        }

        static void UserInfo(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }
            std::string user = GetUsername(req);
            if (req.method == "GET")
            {
                std::unordered_map<std::string, std::string> placeholders;
                rsp.body = LoadTemplate("user_info.html", placeholders);
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                return;
            }
            else if (req.method == "POST")
            {
                auto params = req.params;
                if (params.count("new_username"))
                {
                    std::string new_user = params.find("new_username")->second;
                    if (_user_mgr->UserExists(new_user))
                    {
                        rsp.body = R"(<script>alert("新用户名已存在！"); history.back();</script>)";
                    }
                    else
                    {
                        _user_mgr->UpdateUsername(user, new_user);
                        rsp.set_header("Set-Cookie", "username=" + new_user + "; Max-Age=3600; Path=/");
                        rsp.body = R"(<script>alert("用户名修改成功！"); window.location='/user_info';</script>)";
                    }
                }
                else if (params.count("current_password"))
                {
                    std::string current = params.find("current_password")->second;
                    std::string new_pass = params.find("new_password")->second;
                    std::string confirm = params.find("confirm_password")->second;
                    std::string stored;
                    if (_user_mgr->GetPassword(user, &stored) && stored == current && new_pass == confirm)
                    {
                        _user_mgr->UpdatePassword(user, new_pass);
                        rsp.body = R"(<script>alert("密码修改成功！"); window.location='/login';</script>)";
                    }
                    else
                    {
                        rsp.body = R"(<script>alert("密码修改失败！"); history.back();</script>)";
                    }
                }
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
            }
        }

        static void Upload(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }

            if (!req.is_multipart_form_data())
            {
                rsp.body = R"(<script>alert("上传请求格式错误！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            if (!req.has_file("file"))
            {
                rsp.body = R"(<script>alert("没有选择文件！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            auto file = req.get_file_value("file");

            std::string filename = file.filename;
            std::string file_content = file.content;

            if (filename.empty())
            {
                rsp.body = R"(<script>alert("文件名不能为空！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            // 简单处理路径穿越，避免文件名中带 ../
            while (filename.find("/") != std::string::npos)
            {
                filename = filename.substr(filename.find("/") + 1);
            }
            while (filename.find("\\") != std::string::npos)
            {
                filename = filename.substr(filename.find("\\") + 1);
            }

            const size_t max_file_size = 50 * 1024 * 1024;
            if (file_content.size() > max_file_size)
            {
                rsp.body = R"(<script>alert("文件过大，最大只能上传 50MB！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 413;
                return;
            }

            std::string user = GetUsername(req);

            std::string partition = "private";
            if (req.has_param("partition"))
            {
                partition = req.get_param_value("partition");
            }

            if (partition != "private" && partition != "public")
            {
                partition = "private";
            }

            Config *conf = Config::GetInstance();

            std::string user_dir = (partition == "private"
                                        ? conf->GetPrivateDirPrefix() + user + "/"
                                        : "public/");

            std::string url_path = conf->GetDownloadPrefix() + user_dir + filename;

            BackupInfo old_info;
            if (_data->GetOneByURL(url_path, &old_info))
            {
                rsp.body = R"(<script>alert("文件已存在！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 409;
                return;
            }

            BackupInfo info;
            if (!info.NewBackupInfoForDB(filename, file_content, user, partition))
            {
                rsp.body = R"(<script>alert("文件信息初始化失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            if (!_data->Insert(info))
            {
                rsp.body = R"(<script>alert("上传失败：写入数据库错误！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            std::string redirect_path = (partition == "private" ? "/main/private/" : "/main/public/");
            rsp.status = 302;
            rsp.set_header("Location", redirect_path + "?msg=upload_success");
        }

        static void MainPage(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }
            std::string user = GetUsername(req);

            // 获取分页参数
            int page = 1;
            int page_size = 10;
            if (req.has_param("page"))
            {
                try
                {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1)
                        page = 1;
                }
                catch (...)
                {
                    page = 1;
                }
            }

            // 读取所有备份信息
            std::vector<BackupInfo> all;
            _data->GetAll(&all);

            // 分离私有和公共文件，并按时间排序（最新的在前）
            std::vector<BackupInfo> private_files;
            std::vector<BackupInfo> public_files;

            for (const auto &info : all)
            {
                if (info.partition == "private")
                {
                    // 只包含当前用户的私有文件或管理员可以看到所有
                    if (info.uploader == user || IsAdmin(user))
                    {
                        private_files.push_back(info);
                    }
                }
                else if (info.partition == "public")
                {
                    public_files.push_back(info);
                }
            }

            // 按上传时间排序（最新的在前）
            std::sort(private_files.begin(), private_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });

            // 计算分页信息（私有和公共分别分页）
            int private_total = private_files.size();
            int private_total_pages = (private_total + page_size - 1) / page_size;
            int private_start = (page - 1) * page_size;
            int private_end = std::min(private_start + page_size, private_total);

            int public_total = public_files.size();
            int public_total_pages = (public_total + page_size - 1) / page_size;
            int public_start = (page - 1) * page_size;
            int public_end = std::min(public_start + page_size, public_total);

            // 构建私有文件列表 HTML（当前页）
            std::string private_table;
            for (int i = private_start; i < private_end; i++)
            {
                const auto &info = private_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                // std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path) + "&partition=private&page=" + std::to_string(page);
                std::string download_count_str = std::to_string(info.download_count);

                private_table += "<tr>";
                private_table += "<td>" + EscapeHTML(fname) + "</td>";
                private_table += "<td>" + size_str + "</td>";
                private_table += "<td>" + time_str + "</td>";
                private_table += "<td>" + download_count_str + "</td>";
                private_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                private_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a></td>";
                private_table += "</tr>";
            }

            // 构建公共文件列表 HTML（当前页）
            std::string public_table;
            for (int i = public_start; i < public_end; i++)
            {
                const auto &info = public_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                // std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path) + "&partition=public&page=" + std::to_string(page);
                std::string download_count_str = std::to_string(info.download_count);

                public_table += "<tr>";
                public_table += "<td>" + EscapeHTML(fname) + "</td>";
                public_table += "<td>" + EscapeHTML(info.uploader) + "</td>";
                public_table += "<td>" + size_str + "</td>";
                public_table += "<td>" + time_str + "</td>";
                public_table += "<td>" + download_count_str + "</td>";
                public_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                // 只有上传者或管理员可以删除
                if (info.uploader == user || IsAdmin(user))
                {
                    public_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a>";
                }
                public_table += "</td>";
                public_table += "</tr>";
            }

            if (private_table.empty())
                private_table = "<tr><td colspan=\"5\">暂无私有文件</td></tr>";
            if (public_table.empty())
                public_table = "<tr><td colspan=\"6\">暂无公共文件</td></tr>";

            // 生成分页控件
            PaginationInfo private_pagination;
            private_pagination.current_page = page;
            private_pagination.total_pages = private_total_pages > 0 ? private_total_pages : 1;
            private_pagination.page_size = page_size;
            private_pagination.total_items = private_total;
            std::string private_pagination_html = GeneratePagination(private_pagination, "/main/");

            PaginationInfo public_pagination;
            public_pagination.current_page = page;
            public_pagination.total_pages = public_total_pages > 0 ? public_total_pages : 1;
            public_pagination.page_size = page_size;
            public_pagination.total_items = public_total;
            std::string public_pagination_html = GeneratePagination(public_pagination, "/main/");

            // 使用 main.html 模板
            std::unordered_map<std::string, std::string> placeholders;
            placeholders["username"] = EscapeHTML(user);
            placeholders["private_table"] = private_table;
            placeholders["public_table"] = public_table;
            placeholders["private_pagination"] = private_pagination_html;
            placeholders["public_pagination"] = public_pagination_html;
            placeholders["search_keyword"] = "";
            placeholders["search_return_button"] = "";
            placeholders["show_private"] = "false";
            placeholders["show_public"] = "false";

            rsp.body = LoadTemplate("main.html", placeholders);
            rsp.set_header("Content-Type", "text/html; charset=UTF-8");
        }

        static void PrivatePage(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }
            std::string user = GetUsername(req);

            // 获取分页参数
            int page = 1;
            int page_size = 10;
            if (req.has_param("page"))
            {
                try
                {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1)
                        page = 1;
                }
                catch (...)
                {
                    page = 1;
                }
            }

            // 读取所有备份信息
            std::vector<BackupInfo> all;
            _data->GetAll(&all);

            // 分离私有和公共文件，并按时间排序
            std::vector<BackupInfo> private_files;
            std::vector<BackupInfo> public_files;

            for (const auto &info : all)
            {
                if (info.partition == "private")
                {
                    if (info.uploader == user || IsAdmin(user))
                    {
                        private_files.push_back(info);
                    }
                }
                else if (info.partition == "public")
                {
                    public_files.push_back(info);
                }
            }

            // 按上传时间排序（最新的在前）
            std::sort(private_files.begin(), private_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });

            // 计算分页信息
            int private_total = private_files.size();
            int private_total_pages = (private_total + page_size - 1) / page_size;
            int private_start = (page - 1) * page_size;
            int private_end = std::min(private_start + page_size, private_total);

            int public_total = public_files.size();
            int public_total_pages = (public_total + page_size - 1) / page_size;
            int public_start = (page - 1) * page_size;
            int public_end = std::min(public_start + page_size, public_total);

            // 构建文件列表 HTML（当前页）
            std::string private_table;
            for (int i = private_start; i < private_end; i++)
            {
                const auto &info = private_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                private_table += "<tr>";
                private_table += "<td>" + EscapeHTML(fname) + "</td>";
                private_table += "<td>" + size_str + "</td>";
                private_table += "<td>" + time_str + "</td>";
                private_table += "<td>" + download_count_str + "</td>";
                private_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                private_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a></td>";
                private_table += "</tr>";
            }

            std::string public_table;
            for (int i = public_start; i < public_end; i++)
            {
                const auto &info = public_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                public_table += "<tr>";
                public_table += "<td>" + EscapeHTML(fname) + "</td>";
                public_table += "<td>" + EscapeHTML(info.uploader) + "</td>";
                public_table += "<td>" + size_str + "</td>";
                public_table += "<td>" + time_str + "</td>";
                public_table += "<td>" + download_count_str + "</td>";
                public_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                if (info.uploader == user || IsAdmin(user))
                {
                    public_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a>";
                }
                public_table += "</td>";
                public_table += "</tr>";
            }

            if (private_table.empty())
                private_table = "<tr><td colspan=\"5\">暂无私有文件</td></tr>";
            if (public_table.empty())
                public_table = "<tr><td colspan=\"6\">暂无公共文件</td></tr>";

            // 生成分页控件
            PaginationInfo private_pagination;
            private_pagination.current_page = page;
            private_pagination.total_pages = private_total_pages > 0 ? private_total_pages : 1;
            private_pagination.page_size = page_size;
            private_pagination.total_items = private_total;
            std::string private_pagination_html = GeneratePagination(private_pagination, "/main/private/");

            PaginationInfo public_pagination;
            public_pagination.current_page = page;
            public_pagination.total_pages = public_total_pages > 0 ? public_total_pages : 1;
            public_pagination.page_size = page_size;
            public_pagination.total_items = public_total;
            std::string public_pagination_html = GeneratePagination(public_pagination, "/main/private/");

            std::unordered_map<std::string, std::string> placeholders;
            placeholders["username"] = EscapeHTML(user);
            placeholders["private_table"] = private_table;
            placeholders["public_table"] = public_table;
            placeholders["private_pagination"] = private_pagination_html;
            placeholders["public_pagination"] = public_pagination_html;
            placeholders["search_keyword"] = "";
            placeholders["search_return_button"] = "";
            placeholders["show_private"] = "true";
            placeholders["show_public"] = "false";

            rsp.body = LoadTemplate("main.html", placeholders);
            rsp.set_header("Content-Type", "text/html; charset=UTF-8");
        }

        static void PublicPage(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }
            std::string user = GetUsername(req);

            // 获取分页参数
            int page = 1;
            int page_size = 10;
            if (req.has_param("page"))
            {
                try
                {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1)
                        page = 1;
                }
                catch (...)
                {
                    page = 1;
                }
            }

            // 读取所有备份信息
            std::vector<BackupInfo> all;
            _data->GetAll(&all);

            // 分离私有和公共文件，并按时间排序
            std::vector<BackupInfo> private_files;
            std::vector<BackupInfo> public_files;

            for (const auto &info : all)
            {
                if (info.partition == "private")
                {
                    if (info.uploader == user || IsAdmin(user))
                    {
                        private_files.push_back(info);
                    }
                }
                else if (info.partition == "public")
                {
                    public_files.push_back(info);
                }
            }

            // 按上传时间排序（最新的在前）
            std::sort(private_files.begin(), private_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });

            // 计算分页信息
            int private_total = private_files.size();
            int private_total_pages = (private_total + page_size - 1) / page_size;
            int private_start = (page - 1) * page_size;
            int private_end = std::min(private_start + page_size, private_total);

            int public_total = public_files.size();
            int public_total_pages = (public_total + page_size - 1) / page_size;
            int public_start = (page - 1) * page_size;
            int public_end = std::min(public_start + page_size, public_total);

            // 构建文件列表 HTML（当前页）
            std::string private_table;
            for (int i = private_start; i < private_end; i++)
            {
                const auto &info = private_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                private_table += "<tr>";
                private_table += "<td>" + EscapeHTML(fname) + "</td>";
                private_table += "<td>" + size_str + "</td>";
                private_table += "<td>" + time_str + "</td>";
                private_table += "<td>" + download_count_str + "</td>";
                private_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                private_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a></td>";
                private_table += "</tr>";
            }

            std::string public_table;
            for (int i = public_start; i < public_end; i++)
            {
                const auto &info = public_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                public_table += "<tr>";
                public_table += "<td>" + EscapeHTML(fname) + "</td>";
                public_table += "<td>" + EscapeHTML(info.uploader) + "</td>";
                public_table += "<td>" + size_str + "</td>";
                public_table += "<td>" + time_str + "</td>";
                public_table += "<td>" + download_count_str + "</td>";
                public_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                if (info.uploader == user || IsAdmin(user))
                {
                    public_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a>";
                }
                public_table += "</td>";
                public_table += "</tr>";
            }

            if (private_table.empty())
                private_table = "<tr><td colspan=\"5\">暂无私有文件</td></tr>";
            if (public_table.empty())
                public_table = "<tr><td colspan=\"6\">暂无公共文件</td></tr>";

            // 生成分页控件
            PaginationInfo private_pagination;
            private_pagination.current_page = page;
            private_pagination.total_pages = private_total_pages > 0 ? private_total_pages : 1;
            private_pagination.page_size = page_size;
            private_pagination.total_items = private_total;
            std::string private_pagination_html = GeneratePagination(private_pagination, "/main/public/");

            PaginationInfo public_pagination;
            public_pagination.current_page = page;
            public_pagination.total_pages = public_total_pages > 0 ? public_total_pages : 1;
            public_pagination.page_size = page_size;
            public_pagination.total_items = public_total;
            std::string public_pagination_html = GeneratePagination(public_pagination, "/main/public/");

            std::unordered_map<std::string, std::string> placeholders;
            placeholders["username"] = EscapeHTML(user);
            placeholders["private_table"] = private_table;
            placeholders["public_table"] = public_table;
            placeholders["private_pagination"] = private_pagination_html;
            placeholders["public_pagination"] = public_pagination_html;
            placeholders["search_keyword"] = "";
            placeholders["search_return_button"] = "";
            placeholders["show_private"] = "false";
            placeholders["show_public"] = "true";

            rsp.body = LoadTemplate("main.html", placeholders);
            rsp.set_header("Content-Type", "text/html; charset=UTF-8");
        }

        static void Search(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }
            // 支持 keyword 和 query 两种参数名
            std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : (req.has_param("query") ? req.get_param_value("query") : "");

            // 如果搜索关键词为空，重定向到主页面
            if (keyword.empty())
            {
                rsp.status = 302;
                rsp.set_header("Location", "/main/");
                return;
            }

            // 获取分页参数
            int page = 1;
            int page_size = 10;
            if (req.has_param("page"))
            {
                try
                {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1)
                        page = 1;
                }
                catch (...)
                {
                    page = 1;
                }
            }

            std::string user = GetUsername(req);

            std::vector<BackupInfo> all;
            _data->GetAll(&all);

            // 分离匹配的私有和公共文件
            std::vector<BackupInfo> private_files;
            std::vector<BackupInfo> public_files;

            for (const auto &info : all)
            {
                std::string fname = info.file_name;
                // 文件名不匹配则跳过
                if (fname.find(keyword) == std::string::npos)
                    continue;

                if (info.partition == "private")
                {
                    // 权限过滤：只包含当前用户的私有文件或管理员可以看到所有
                    if (info.uploader == user || IsAdmin(user))
                    {
                        private_files.push_back(info);
                    }
                }
                else if (info.partition == "public")
                {
                    public_files.push_back(info);
                }
            }

            // 按上传时间排序（最新的在前）
            std::sort(private_files.begin(), private_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(),
                      [](const BackupInfo &a, const BackupInfo &b)
                      { return a.mtime > b.mtime; });

            // 计算分页信息
            int private_total = private_files.size();
            int private_total_pages = (private_total + page_size - 1) / page_size;
            int private_start = (page - 1) * page_size;
            int private_end = std::min(private_start + page_size, private_total);

            int public_total = public_files.size();
            int public_total_pages = (public_total + page_size - 1) / page_size;
            int public_start = (page - 1) * page_size;
            int public_end = std::min(public_start + page_size, public_total);

            // 构建文件列表 HTML（当前页）
            std::string private_table;
            for (int i = private_start; i < private_end; i++)
            {
                const auto &info = private_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                private_table += "<tr>";
                private_table += "<td>" + EscapeHTML(fname) + "</td>";
                private_table += "<td>" + size_str + "</td>";
                private_table += "<td>" + time_str + "</td>";
                private_table += "<td>" + download_count_str + "</td>";
                private_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                private_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a></td>";
                private_table += "</tr>";
            }

            std::string public_table;
            for (int i = public_start; i < public_end; i++)
            {
                const auto &info = public_files[i];
                std::string fname = info.file_name;
                std::string size_str = std::to_string(info.fsize / 1024) + " KB";
                std::string time_str = FormatTime(info.mtime);
                std::string download_link = info.url_path;
                std::string delete_link = "/delete?url=" + httplib::detail::encode_url(info.url_path);
                std::string download_count_str = std::to_string(info.download_count);

                public_table += "<tr>";
                public_table += "<td>" + EscapeHTML(fname) + "</td>";
                public_table += "<td>" + EscapeHTML(info.uploader) + "</td>";
                public_table += "<td>" + size_str + "</td>";
                public_table += "<td>" + time_str + "</td>";
                public_table += "<td>" + download_count_str + "</td>";
                public_table += "<td><a href=\"" + download_link + "\" class=\"download-btn\">下载</a> ";
                if (info.uploader == user || IsAdmin(user))
                {
                    public_table += "<a href=\"" + delete_link + "\" class=\"delete-btn\" onclick=\"return confirm('确定删除？');\">删除</a>";
                }
                public_table += "</td>";
                public_table += "</tr>";
            }

            if (private_table.empty())
                private_table = "<tr><td colspan=\"5\">无匹配的私有文件</td></tr>";
            if (public_table.empty())
                public_table = "<tr><td colspan=\"6\">无匹配的公共文件</td></tr>";

            // 生成分页控件（搜索时保留关键词参数）
            std::string search_params = "&keyword=" + httplib::detail::encode_url(keyword);
            PaginationInfo private_pagination;
            private_pagination.current_page = page;
            private_pagination.total_pages = private_total_pages > 0 ? private_total_pages : 1;
            private_pagination.page_size = page_size;
            private_pagination.total_items = private_total;
            std::string private_pagination_html = GeneratePagination(private_pagination, "/search", search_params);

            PaginationInfo public_pagination;
            public_pagination.current_page = page;
            public_pagination.total_pages = public_total_pages > 0 ? public_total_pages : 1;
            public_pagination.page_size = page_size;
            public_pagination.total_items = public_total;
            std::string public_pagination_html = GeneratePagination(public_pagination, "/search", search_params);

            std::unordered_map<std::string, std::string> placeholders;
            placeholders["username"] = EscapeHTML(user);
            placeholders["private_table"] = private_table;
            placeholders["public_table"] = public_table;
            placeholders["private_pagination"] = private_pagination_html;
            placeholders["public_pagination"] = public_pagination_html;
            placeholders["search_keyword"] = EscapeHTML(keyword);
            placeholders["search_return_button"] = "<button onclick=\"window.location.href='/main/'\">返回</button>";
            placeholders["show_private"] = "false";
            placeholders["show_public"] = "false";

            rsp.body = LoadTemplate("main.html", placeholders);
            rsp.set_header("Content-Type", "text/html; charset=UTF-8");
        }

        static void DeleteFile(const httplib::Request &req, httplib::Response &rsp)
        {
            if (!IsLoggedIn(req))
            {
                rsp.status = 302;
                rsp.set_header("Location", "/login");
                return;
            }

            std::string url = req.has_param("url") ? req.get_param_value("url") : "";
            if (url.empty())
            {
                rsp.body = R"(<script>alert("无效URL！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            BackupInfo info;
            if (!_data->GetOneByURL(url, &info))
            {
                rsp.body = R"(<script>alert("文件未找到！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 404;
                return;
            }

            std::string user = GetUsername(req);
            if (info.uploader != user && !IsAdmin(user))
            {
                rsp.body = R"(<script>alert("无权限删除！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 403;
                return;
            }

            if (!_data->Delete(info))
            {
                rsp.body = R"(<script>alert("删除失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            rsp.status = 302;

            std::string partition = req.has_param("partition") ? req.get_param_value("partition") : "";
            std::string page = req.has_param("page") ? req.get_param_value("page") : "";

            std::string redirect_url = "/main/";

            if (!partition.empty())
            {
                redirect_url += partition + "/";
                if (!page.empty())
                {
                    redirect_url += "?page=" + page;
                }
            }
            else
            {
                redirect_url += "?msg=delete_success";
            }

            rsp.set_header("Location", redirect_url);
        }

        static std::string GetETag(const BackupInfo &info)
        {
            std::string etag = info.file_name;
            etag += "-" + std::to_string(info.fsize);
            etag += "-" + std::to_string(info.mtime);
            return etag;
        }

        static void Download(const httplib::Request &req, httplib::Response &rsp)
        {
            BackupInfo info;
            if (!_data->GetOneByURL(req.path, &info))
            {
                rsp.status = 404;
                rsp.body = "File not found";
                return;
            }

            // 私有文件必须登录，并且只能本人或管理员下载
            if (info.partition == "private")
            {
                if (!IsLoggedIn(req))
                {
                    rsp.status = 302;
                    rsp.set_header("Location", "/login");
                    return;
                }

                std::string user = GetUsername(req);
                if (info.uploader != user && !IsAdmin(user))
                {
                    rsp.status = 403;
                    rsp.body = "无权限访问该文件";
                    return;
                }
            }

            time_t now = time(nullptr);
            info.atime = now;
            info.download_count++;

            rsp.body = info.file_data;
            rsp.set_header("Accept-Ranges", "bytes");
            rsp.set_header("ETag", GetETag(info));
            rsp.set_header("Content-Type", "application/octet-stream");

            std::string disposition = "attachment; filename=\"" + info.file_name + "\"";
            rsp.set_header("Content-Disposition", disposition);

            rsp.status = 200;

            _data->Update(info);
        }

    public:
        Service()
        {
            Config *conf = Config::GetInstance();
            _server_port = conf->GetServerPort();
            _server_ip = conf->GetServerIp();
            _download_prefix = conf->GetDownloadPrefix();
        }
        bool RunModule()
        {
            _server.Get("/login", Service::Login);
            _server.Post("/login", Service::Login);
            _server.Get("/register", Service::Register);
            _server.Post("/register", Service::Register);
            _server.Get("/user_info", Service::UserInfo);
            _server.Post("/user_info", Service::UserInfo);
            _server.Post("/upload", Service::Upload);
            _server.Get("/main/", Service::MainPage);
            _server.Get("/main/private/", Service::PrivatePage);
            _server.Get("/main/public/", Service::PublicPage);
            _server.Get("/search", Service::Search);
            _server.Get("/delete", Service::DeleteFile);
            _server.Get("/", [](const httplib::Request &req, httplib::Response &rsp)
                        {
            rsp.status = 302;
            rsp.set_header("Location", "/main/"); });
            _server.Get("/logout", Service::Logout);

            _server.Get("/main", [](const httplib::Request &req, httplib::Response &rsp)
                        {
            rsp.status = 302;
            rsp.set_header("Location", "/main/"); });

            std::string download_url = _download_prefix + "(.*)";
            _server.Get(download_url, Service::Download);

            _server.listen(_server_ip.c_str(), _server_port);

            return true;
        }
    };

    const std::string Service::page_template = R"(
<!DOCTYPE html>
<html lang="zh">
<head>
    <meta charset="UTF-8">
    <title>PAGE_TITLE_PLACEHOLDER</title>
    <style>
        body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            background: linear-gradient(to bottom, #f4f4f9, #e0e0e0); 
            display: flex; 
            justify-content: center; 
            align-items: center; 
            height: 100vh; 
            margin: 0; 
        }
        .container { 
            background: white; 
            padding: 30px; 
            border-radius: 12px; 
            box-shadow: 0 4px 20px rgba(0,0,0,0.15); 
            width: 600px; 
            text-align: center; 
            transition: transform 0.3s ease; 
        }
        .container:hover { transform: translateY(-5px); }
        h2 { 
            color: #0056b3; 
            font-size: 1.8em; 
            margin-bottom: 15px; 
        }
        input[type="file"], input[type="submit"] { 
            width: 100%; 
            padding: 12px; 
            margin: 12px 0; 
            border: 1px solid #ccc; 
            border-radius: 6px; 
            box-sizing: border-box; 
            transition: border-color 0.3s ease; 
        }
        input[type="file"]:focus { border-color: #007bff; }
        input[type="submit"] { 
            background: linear-gradient(to right, #007bff, #0056b3); 
            color: white; 
            border: none; 
            cursor: pointer; 
            font-size: 16px; 
            transition: background 0.3s ease; 
        }
        input[type="submit"]:hover { background: linear-gradient(to right, #0056b3, #003d80); }
        table { 
            width: 100%; 
            border-collapse: collapse; 
            margin-top: 20px; 
        }
        th, td { 
            border: 1px solid #ddd; 
            padding: 8px; 
            text-align: left; 
        }
        th { background-color: #f2f2f2; }
        a { color: #007bff; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="container">
        <h2>欢迎，USERNAME_PLACEHOLDER！</h2>
        <a href="/main/private/">私有区</a> | <a href="/main/public/">公共区</a> | <a href="/user_info">用户信息</a> | <a href="/logout">登出</a>
        
        <h2>搜索文件</h2>
        <form action="/search" method="get">
            <input type="text" name="query" placeholder="输入文件名或关键字">
            <input type="submit" value="搜索">
        </form>

        <h2>上传文件</h2>
        <form id="uploadForm" action="/upload" method="post" enctype="multipart/form-data">
            <input type="file" name="file"><br>
            <input type="radio" name="partition" value="private" checked> 私有区
            <input type="radio" name="partition" value="public"> 公共区<br>
            <input type="submit" value="上传">
        </form>

        <h2>FILE_LIST_TITLE_PLACEHOLDER</h2>
        <table>
            <tr>
                <th>文件名</th>
                <th>大小</th>
                <th>上传时间</th>
                <th>操作</th>
            </tr>
            FILE_LIST_PLACEHOLDER
        </table>
    </div>

    <script>
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            e.preventDefault();
            const form = e.target;
            const formData = new FormData(form);
            fetch('/upload', {
                method: 'POST',
                body: formData
            }).then(response => {
                if (response.ok) {
                    alert('上传成功！');
                    window.location.reload();
                } else {
                    response.text().then(text => alert('上传失败：' + text));
                }
            }).catch(error => {
                alert('上传错误：' + error);
            });
        });
    </script>
</body>
</html>
)";
}
