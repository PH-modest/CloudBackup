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
            "templates/" + filename,     // templates 目录（优先）
            "./templates/" + filename,   // 当前目录下的 templates
            filename,                    // 当前目录
            "./" + filename,             // 当前目录（显式）
            "../src/templates/" + filename,  // 从上级目录的src/templates
            "src/templates/" + filename,     // src/templates子目录
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
            return user == "admin";
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
            else if (req.method == "POST")
            {
                auto params = req.params;
                auto it_user = params.find("username");
                auto it_pass = params.find("password");
                if (it_user != params.end() && it_pass != params.end())
                {
                    std::string user = it_user->second;
                    std::string pass = it_pass->second;
                    std::string stored_pass;
                    if (_user_mgr->GetPassword(user, &stored_pass) && stored_pass == pass)
                    {
                        rsp.set_header("Set-Cookie", "username=" + user + "; Max-Age=3600; Path=/");
                        rsp.status = 302;
                        rsp.set_header("Location", "/main/");
                        return;
                    }
                }
                rsp.status = 401;
                rsp.body = R"(<script>alert("登录失败！"); window.location='/login';</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
            }
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
            else if (req.method == "POST")
            {
                auto params = req.params;
                auto it_user = params.find("username");
                auto it_pass = params.find("password");
                auto it_confirm = params.find("confirm_password");
                if (it_user != params.end() && it_pass != params.end() && it_confirm != params.end())
                {
                    std::string user = it_user->second;
                    std::string pass = it_pass->second;
                    std::string confirm = it_confirm->second;
                    if (pass != confirm)
                    {
                        rsp.body = R"(<script>alert("密码不匹配！"); history.back();</script>)";
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
                    _user_mgr->AddUser(user, pass);
                    rsp.status = 302;
                    rsp.set_header("Location", "/login");
                    return;
                }
                rsp.body = R"(<script>alert("注册失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
            }
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

            std::string user = GetUsername(req);
            if (!req.has_header("Content-Type") || req.get_header_value("Content-Type").find("multipart/form-data") == std::string::npos)
            {
                std::cerr << "Invalid Content-Type: " << req.get_header_value("Content-Type")
                          << ", Body size: " << req.body.size() << std::endl;
                rsp.body = R"(<script>alert("上传失败：无效的请求格式！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            if (req.files.empty() || req.files.find("file") == req.files.end())
            {
                std::cerr << "No file found in request." << std::endl;
                rsp.body = R"(<script>alert("上传失败：没有选择文件！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            const auto &uploaded_file = req.files.find("file")->second;
            std::string orig_filename = uploaded_file.filename;
            std::string file_content = uploaded_file.content;

            if (orig_filename.empty() || file_content.empty())
            {
                std::cerr << "Invalid file: Filename: " << orig_filename << ", Size: " << file_content.size() << std::endl;
                rsp.body = R"(<script>alert("上传失败：文件无效或为空！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            if (file_content.size() > 100 * 1024 * 1024)
            { // 添加大小上限检查，防止内存溢出
                std::cerr << "File too large: " << file_content.size() << std::endl;
                rsp.body = R"(<script>alert("文件太大！上限100MB"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 413;
                return;
            }

            // Sanitize filename: 只清理危险字符（路径分隔符、控制字符等），保留中文字符
            std::string filename = orig_filename;
            // 移除路径分隔符和其他危险字符
            std::string dangerous_chars = "/\\:*?\"<>|";
            for (char c : dangerous_chars)
            {
                size_t pos = 0;
                while ((pos = filename.find(c, pos)) != std::string::npos)
                {
                    filename.replace(pos, 1, "_");
                    pos++;
                }
            }
            // 移除控制字符（ASCII 0-31，除了常见的空白字符）
            for (int i = 0; i < 32; i++)
            {
                if (i != 9 && i != 10 && i != 13) // 保留 tab, newline, carriage return
                {
                    char c = static_cast<char>(i);
                    size_t pos = 0;
                    while ((pos = filename.find(c, pos)) != std::string::npos)
                    {
                        filename.replace(pos, 1, "_");
                        pos++;
                    }
                }
            }
            // 移除文件名末尾的点或空格（Windows 不允许）
            while (!filename.empty() && (filename.back() == '.' || filename.back() == ' '))
            {
                filename.pop_back();
            }
            if (filename.empty())
            {
                std::cerr << "Sanitized filename empty. Original: " << orig_filename << std::endl;
                rsp.body = R"(<script>alert("文件名无效！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 400;
                return;
            }

            // 从 multipart/form-data 中获取 partition 参数
            // httplib 会将 multipart 中的非文件字段也放入 files map 中
            std::string partition = "private"; // 默认值
            
            // 首先尝试从 params 中获取（URL 参数或已解析的表单数据）
            if (req.has_param("partition"))
            {
                partition = req.get_param_value("partition");
            }
            else
            {
                // 对于 multipart/form-data，非文件字段也会在 files 中
                auto it = req.files.find("partition");
                if (it != req.files.end())
                {
                    partition = it->second.content;
                    // 移除可能的换行符和空白字符
                    if (!partition.empty())
                    {
                        partition.erase(0, partition.find_first_not_of(" \t\r\n"));
                        if (!partition.empty())
                        {
                            partition.erase(partition.find_last_not_of(" \t\r\n") + 1);
                        }
                    }
                }
            }
            
            // 确保 partition 值有效
            if (partition != "public" && partition != "private")
            {
                std::cerr << "Invalid partition value: '" << partition << "', using default: private" << std::endl;
                partition = "private";
            }
            
            std::cerr << "Upload partition: " << partition << " (has_param: " << req.has_param("partition") 
                      << ", files.count: " << req.files.count("partition") << ")" << std::endl;

            Config *conf = Config::GetInstance();
            std::string user_dir = (partition == "private" ? conf->GetPrivateDirPrefix() + user + "/" : partition + "/");
            std::string final_dir = conf->GetBackDir() + user_dir;
            FileUtil fu_dir(final_dir);
            if (!fu_dir.CreateDirectory())
            {
                std::cerr << "Failed to create final dir: " << final_dir << ", errno: " << strerror(errno) << std::endl;
                rsp.body = R"(<script>alert("上传失败：无法创建存储目录！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            std::string final_path = final_dir + filename;
            if (FileUtil(final_path).Exists())
            {
                std::cerr << "File already exists: " << final_path << std::endl;
                rsp.body = R"(<script>alert("文件已存在！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 409;
                return;
            }

            // 直接写入最终路径
            FileUtil fu_final(final_path);
            if (!fu_final.SetContent(file_content))
            {
                std::cerr << "Failed to write file: " << final_path << ", errno: " << strerror(errno) << std::endl;
                rsp.body = R"(<script>alert("上传失败：写入文件错误！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            // 完整性校验
            if (fu_final.FileSize() != static_cast<int64_t>(file_content.size()))
            {
                std::cerr << "Size mismatch: expected " << file_content.size() << ", actual " << fu_final.FileSize() << std::endl;
                fu_final.Remove();
                rsp.body = R"(<script>alert("文件完整性校验失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }

            BackupInfo info;
            if (!info.NewBackupInfo(final_path, user, partition))
            {
                fu_final.Remove();
                rsp.body = R"(<script>alert("备份信息初始化失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
                return;
            }
            _data->Insert(info);

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
                try {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1) page = 1;
                } catch (...) {
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
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(), 
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });

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
                std::string fname = FileUtil(info.real_path).FileName();
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

            // 构建公共文件列表 HTML（当前页）
            std::string public_table;
            for (int i = public_start; i < public_end; i++)
            {
                const auto &info = public_files[i];
                std::string fname = FileUtil(info.real_path).FileName();
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
                try {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1) page = 1;
                } catch (...) {
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
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(), 
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });

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
                std::string fname = FileUtil(info.real_path).FileName();
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
                std::string fname = FileUtil(info.real_path).FileName();
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
                try {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1) page = 1;
                } catch (...) {
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
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(), 
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });

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
                std::string fname = FileUtil(info.real_path).FileName();
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
                std::string fname = FileUtil(info.real_path).FileName();
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
            std::string keyword = req.has_param("keyword") ? req.get_param_value("keyword") : 
                              (req.has_param("query") ? req.get_param_value("query") : "");
            
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
                try {
                    page = std::stoi(req.get_param_value("page"));
                    if (page < 1) page = 1;
                } catch (...) {
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
                std::string fname = FileUtil(info.real_path).FileName();
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
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });
            std::sort(public_files.begin(), public_files.end(), 
                     [](const BackupInfo &a, const BackupInfo &b) { return a.mtime > b.mtime; });

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
                std::string fname = FileUtil(info.real_path).FileName();
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
                std::string fname = FileUtil(info.real_path).FileName();
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
            // 根据 pack_flag 判断文件位置
            // 如果 pack_flag = true，文件在 packdir 中（压缩文件）
            // 如果 pack_flag = false，文件在 backdir 中（源文件）
            bool delete_success = true;
            
            if (info.pack_flag)
            {
                // 文件被压缩，删除 packdir 中的压缩文件
                FileUtil fu_pack(info.pack_path);
                if (fu_pack.Exists())
                {
                    if (!fu_pack.Remove())
                    {
                        std::cerr << "Failed to remove pack_path: " << info.pack_path << ", errno: " << strerror(errno) << std::endl;
                        delete_success = false;
                    }
                }
                else
                {
                    std::cerr << "Compressed file not found: " << info.pack_path << std::endl;
                    delete_success = false;
                }
            }
            else
            {
                // 文件未压缩，删除 backdir 中的源文件
                FileUtil fu_real(info.real_path);
                if (fu_real.Exists())
                {
                    if (!fu_real.Remove())
            {
                std::cerr << "Failed to remove real_path: " << info.real_path << ", errno: " << strerror(errno) << std::endl;
                delete_success = false;
            }
                }
                else
            {
                    std::cerr << "Source file not found: " << info.real_path << std::endl;
                delete_success = false;
                }
            }
            
            if (!delete_success)
            {
                rsp.body = R"(<script>alert("文件在磁盘上未找到或删除失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 404;
                return;
            }

            if (delete_success && _data->Delete(info))
            {
                rsp.status = 302;
                // 删除后重定向到主页面，显示所有文件
                rsp.set_header("Location", "/main/?msg=delete_success");
            }
            else
            {
                rsp.body = R"(<script>alert("删除失败！"); history.back();</script>)";
                rsp.set_header("Content-Type", "text/html; charset=UTF-8");
                rsp.status = 500;
            }
        }

        static std::string GetETag(const BackupInfo &info)
        {
            FileUtil fu(info.real_path);
            std::string etag = fu.FileName();
            etag += "-" + std::to_string(info.fsize) + "-" + std::to_string(info.mtime);
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
            
            // 如果文件被压缩，需要从 packdir 解压到 backdir
            if (info.pack_flag)
            {
                // 压缩文件在 packdir 中
                FileUtil fu_pack(info.pack_path);
                if (!fu_pack.Exists())
                {
                    rsp.status = 404;
                    rsp.body = "Compressed file not found";
                    return;
                }
                
                // 确保 backdir 中的目标目录存在
                std::string real_dir = info.real_path.substr(0, info.real_path.find_last_of('/'));
                FileUtil(real_dir).CreateDirectory();

                // 解压文件到 backdir
                if (!fu_pack.UnCompress(info.real_path))
                {
                    rsp.status = 500;
                    rsp.body = "Failed to decompress file";
                    return;
                }
                
                // 解压成功后，删除压缩文件（下次长时间未访问会再次压缩）
                if (!fu_pack.Remove())
                {
                    std::cerr << "Failed to remove compressed file: " << info.pack_path << std::endl;
                }
                
                // 更新文件信息
                FileUtil fu_real(info.real_path);
                info.pack_flag = false;
                info.fsize = fu_real.FileSize();
                info.mtime = fu_real.LastMTime();
                info.atime = time(nullptr); // 更新访问时间为当前时间
                _data->Update(info);
            }
            
            // 从 backdir 读取文件（此时文件一定在 backdir 中）
            FileUtil fu(info.real_path);
            if (!fu.Exists())
            {
                rsp.status = 404;
                rsp.body = "File not found in backdir";
                return;
            }

            // 更新文件的访问时间
            time_t now = time(nullptr);
            #ifdef _WIN32
                // Windows 上更新访问时间
                struct _utimbuf ut;
                ut.actime = now;
                ut.modtime = fu.LastMTime();
                _utime(info.real_path.c_str(), &ut);
            #else
                // Linux 上更新访问时间
                struct utimbuf ut;
                ut.actime = now;
                ut.modtime = fu.LastMTime();
                utime(info.real_path.c_str(), &ut);
            #endif
            
            // 更新 BackupInfo 中的访问时间
            info.atime = now;

            bool retrans = false;
            std::string old_etag;
            if (req.has_header("If-Range"))
            {
                old_etag = req.get_header_value("If-Range");
                if (old_etag == GetETag(info))
                {
                    retrans = true;
                }
            }

            fu.GetContent(&rsp.body);
            rsp.set_header("Accept-Ranges", "bytes");
            rsp.set_header("ETag", GetETag(info));
            rsp.set_header("Content-Type", "application/octet-stream");
            rsp.status = retrans ? 206 : 200;
            
            // 更新下载计数和访问时间
            info.download_count++;
            _data->Update(info);
        }

    public:
        Service()
        {
            Config *conf = Config::GetInstance();
            _server_port = conf->GetServerPort();
            _server_ip = conf->GetServerIp();
            _download_prefix = conf->GetDownloadPrefix();

            std::string back_public = conf->GetBackDir() + "public/";
            std::string pack_public = conf->GetPackDir() + "public/";
            FileUtil(back_public).CreateDirectory();
            FileUtil(pack_public).CreateDirectory();
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
