#pragma once
#include "data.hpp" //上传文件需要备份，就需要将文件放入进去
#include "httplib.h"
#include <algorithm>

extern cloud::DataManager *_data;

namespace cloud
{
    class Service
    {
    private:
        int _server_port;
        std::string _server_ip;
        std::string _download_prefix;
        httplib::Server _server;

    private:
        static void Upload(const httplib::Request &req, httplib::Response &rsp)
        {
            // printf("----------Upload-----------\n");
            // post /upload    文件数据在正文中（正文并不全是文件数据）
            // 首先判断有没有上传的文件区域
            auto ret = req.has_file("file");
            if (ret == false)
            {
                rsp.status = 400;
                return;
            }
            // DEGUG printf("----------有文件-----------\n");
            // 获取数据
            const auto &file = req.get_file_value("file");
            // file.filename//文件名称
            // file.content//文件数据
            std::string back_dir = Config::GetInstance()->GetBackDir();
            std::string realpath = back_dir + FileUtil(file.filename).FileName(); // 调用FileUtil匿名对象
            FileUtil fu(realpath);
            fu.SetContent(file.content); // 将数据写入文件中
            BackupInfo info;
            info.NewBackupInfo(realpath); // 组织备份的文件信息
            _data->Insert(info);          // 向数据管理模块添加备份的文件信息
            // DEBUG printf("----------Upload运行结束-----------\n");
            
            //上传成功之后返回首页
            rsp.status = 302;
            rsp.set_header("Location","/");
            return;
        }
        static std::string TimetoStr(time_t t)
        {
            // std::string tmp = std::ctime(&t);
            // return tmp;

            struct tm *local_time = localtime(&t);
            char time_str[100];
            strftime(time_str,sizeof(time_str),"%Y年%m月%d日 %H:%M:%S", local_time);
            return std::string(time_str);
        }

        // 优化文件大小显示：小于1MB用KB（保留1位小数），1MB及以上用MB（保留2位小数）
        static std::string FormatFileSize(size_t fsize)
        {
            // 转换为KB（1KB = 1024字节）
            double size_kb = static_cast<double>(fsize) / 1024.0;
            
            // 如果小于1MB（1024KB），用KB显示
            if (size_kb < 1024.0)
            {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1) << size_kb << "KB";
                return ss.str();
            }
            // 1MB及以上用MB显示
            else
            {
                double size_mb = size_kb / 1024.0;
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << size_mb << "MB";
                return ss.str();
            }
        }

        static void ListShow(const httplib::Request &req, httplib::Response &rsp)
        {
            // 1. 获取所有的文件备份信息
            std::vector<BackupInfo> arry;
            _data->GetAll(&arry);

            //按上传时间排序
            std::sort(arry.begin(),arry.end(),[](const BackupInfo& a,const BackupInfo& b)
            {
                return a.mtime > b.mtime;
            });

            // 2. 组织带上传功能和美化样式的html内容
            std::stringstream ss;
            ss << "<!DOCTYPE html>";
            ss << "<html lang='zh-CN'>";
            ss << "<head>";
            ss << "<meta charset='UTF-8'>";
            ss << "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
            ss << "<title>文件备份系统</title>";
            ss << "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css'>";
            ss << "<style>";
            // 基础样式
            ss << "body { font-family: 'Microsoft YaHei', 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; max-width: 1200px; margin: 0 auto; padding: 20px; background-color: #f5f7fa; color: #333; }";
            ss << "h1 { color: #2c3e50; border-bottom: 2px solid #3498db; padding-bottom: 15px; margin-bottom: 30px; text-align: center; }";

            // 上传表单样式
            ss << ".upload-container { background-color: white; padding: 25px; border-radius: 8px; box-shadow: 0 2px 15px rgba(0,0,0,0.08); margin-bottom: 30px; transition: box-shadow 0.3s ease; }";
            ss << ".upload-container:hover { box-shadow: 0 5px 20px rgba(0,0,0,0.1); }";
            ss << ".upload-form { display: flex; flex-wrap: wrap; gap: 15px; align-items: center; }";
            ss << ".upload-form input[type='file'] { flex: 1; min-width: 250px; padding: 10px; border: 1px solid #ddd; border-radius: 4px; background-color: #f9f9f9; }";
            ss << ".upload-form input[type='submit'] { background-color: #3498db; color: white; border: none; padding: 10px 25px; border-radius: 4px; cursor: pointer; font-weight: 500; transition: all 0.3s ease; }";
            ss << ".upload-form input[type='submit']:hover { background-color: #2980b9; transform: translateY(-2px); }";
            ss << ".upload-form input[type='submit']:active { transform: translateY(0); }";

            // 表格样式
            ss << ".file-table { width: 100%; border-collapse: collapse; box-shadow: 0 2px 15px rgba(0,0,0,0.08); background-color: white; border-radius: 8px; overflow: hidden; }";
            ss << ".file-table th { background-color: #3498db; color: white; padding: 14px 15px; text-align: left; font-weight: 500; }";
            ss << ".file-table td { padding: 14px 15px; border-bottom: 1px solid #ecf0f1; }";
            ss << ".file-table tr:last-child td { border-bottom: none; }";
            ss << ".file-table tr:hover { background-color: #f8f9fa; transition: background-color 0.3s ease; }";
            ss << ".table-container { overflow-x: auto; }";

            // 下载按钮样式
            ss << ".download-btn { display: inline-flex; align-items: center; gap: 5px; background-color: #2ecc71; color: white; border: none; padding: 8px 15px; border-radius: 4px; cursor: pointer; text-decoration: none; font-weight: 500; transition: all 0.3s ease; }";
            ss << ".download-btn:hover { background-color: #27ae60; transform: translateY(-2px); }";
            ss << ".download-btn:active { transform: translateY(0); }";

            // 响应式调整
            ss << "@media (max-width: 768px) {";
            ss << ".file-table { font-size: 14px; }";
            ss << ".file-table th, .file-table td { padding: 10px 8px; }";
            ss << ".upload-form { flex-direction: column; }";
            ss << ".upload-form input[type='file'], .upload-form input[type='submit'] { width: 100%; box-sizing: border-box; }";
            ss << ".download-btn { padding: 6px 10px; font-size: 14px; }";
            ss << "}";
            ss << "</style>";
            ss << "</head>";
            ss << "<body>";
            ss << "<h1>文件备份系统</h1>";

            // 添加文件上传表单
            ss << "<div class='upload-container'>";
            ss << "<form class='upload-form' action='/upload' method='post' enctype='multipart/form-data'>";
            ss << "<input type='file' name='file' accept='*/*' />";
            ss << "<input type='submit' value='上传文件' />";
            ss << "</form>";
            ss << "</div>";

            // 文件列表表格
            ss << "<div class='table-container'>";
            ss << "<table class='file-table'>";
            ss << "<tr><th>文件名</th><th>上传时间</th><th>文件大小</th><th>操作</th></tr>";

            for (auto &a : arry)
            {
                ss << "<tr>";
                std::string filename = FileUtil(a.real_path).FileName();
                ss << "<td>" << filename << "</td>";
                ss << "<td>" << TimetoStr(a.mtime) << "</td>";
                ss << "<td>" << FormatFileSize(a.fsize) << "</td>";
                ss << "<td><a href='" << a.url_path << "' class='download-btn'><i class='fas fa-download'></i> 下载</a></td>";
                ss << "</tr>";
            }

            ss << "</table>";
            ss << "</div>";
            ss << "</body></html>";

            rsp.body = ss.str();
            rsp.set_header("Content-Type", "text/html; charset=UTF-8");
            rsp.status = 200;
            return;
        }

        // //DEBUG
        // static void No(const httplib::Request& req, httplib::Response& rsp)
        // {
        //     rsp.body.clear();
        //     rsp.body = R"(<!DOCTYPE html><html lang="zh"><head>)";
        //     rsp.body += R"(<meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">)";
        //     rsp.body += R"(<title>中央红色字体</title><style>)";
        //     rsp.body += R"(body {height: 100vh;margin: 0;display: flex;justify-content: center;align-items: center;background-color:#f0f0f0;})";
        //     rsp.body += R"(.content {font-size: 48px;color: red;})";
        //     rsp.body += R"(</style></head><body><div class="content">)";
        //     rsp.body += "客户端请求的语法错误，服务器无法理解 400!!!";
        //     rsp.body += R"(</div></body></html>)";
        //     rsp.set_header("Content-Type", "text/html; charset=UTF-8");
        //     rsp.status = 400;
        // }

        // 因为下面的调用函数是静态的
        static std::string GetETag(const BackupInfo &info)
        {
            // etag : filename-fsize-mtime
            FileUtil fu(info.real_path);
            std::string etag = fu.FileName();
            etag += "-";
            etag += std::to_string(info.fsize);
            etag += "-";
            etag += std::to_string(info.mtime);
            return etag;
        }
        static void Download(const httplib::Request &req, httplib::Response &rsp)
        {
            // 1. 获取客户端请求的资源路径path    req.path
            // 2. 根据资源路径，获取文件备份信息
            BackupInfo info;
            _data->GetOneByURL(req.path, &info);
            // 3. 判断文件是否被压缩，如果被压缩，要先解压缩
            if (info.pack_flag == true)
            {
                FileUtil fu(info.pack_path);
                fu.UnCompress(info.real_path); // 将文件解压到备份目录下
                // 4. 删除压缩包，修改备份信息（已经没有被压缩）
                fu.Remove();
                info.pack_flag = false;
                _data->Update(info);
            }
            // 5. 读取文件数据，放入rsp.body中
            FileUtil fu(info.real_path);

            // 如果没有If-Range字段则是正常下载，或者如果有这个字段，
            // 但是他的值与当前文件的etag不一致，则也必须中心返回全部数据
            bool retrans = false; // 标记当前是否是断点续传
            std::string old_etag;
            if (req.has_header("If-Range"))
                ;
            {
                old_etag = req.get_header_value("If-Range");
                // 有If-range字段，且这个字段的值与请求文件的最新etag一致则符合断点续传
                if (old_etag == GetETag(info))
                {
                    retrans = true;
                }
            }

            if (retrans == false)
            {
                fu.GetContent(&rsp.body);
                // 6. 设置响应头部字段：ETag ， Accept-Ranges: bytes
                rsp.set_header("Accept-Ranges:", "bytes");
                rsp.set_header("ETag", GetETag(info));
                rsp.set_header("Content-Type", "application/octet-stream");
                rsp.status = 200;
            }
            else
            {
                // httplib内部实现了对于区间请求也就是断点续传请求的处理
                // 只需要我们用户将文件所有数据读取到rsp.body中，他们内部会自动根据请求区间，从body中取出指定区间数据进行响应
                // std::string range = req.get_header_val("Range"); 解析bytes=start-end
                fu.GetContent(&rsp.body);
                rsp.set_header("Accept-Ranges:", "bytes");
                rsp.set_header("ETag", GetETag(info));
                rsp.set_header("Content-Type", "application/octet-stream");
                // rsp.set_header("Content-Range","bytes start-end/fsize");
                rsp.status = 206;
            }
        }

    public:
        Service()
        {
            // 前三个变量初始化数据可以从配置文件里获取，_server创建时就自动初始化了
            Config *conf = Config::GetInstance();
            _server_port = conf->GetServerPort();
            _server_ip = conf->GetServerIp();
            _download_prefix = conf->GetDownloadPrefix();
        }
        bool RunModule()
        {

            // 创建映射关系
            // DEBUG printf("-----------RunModule-------------\n");
            _server.Post("/upload", Upload);
            // DEBUG printf("-----------Post upload-------------\n");
            _server.Get("/listshow/", ListShow);
            _server.Get("/", ListShow);

            ////DEBUG
            ////设置自定义日志记录器
            // _server.set_logger([](const httplib::Request& req, const httplib::Response& res) {
            //     // 打印请求日志
            //     std::cout << "Request:" << std::endl;
            //     std::cout << "  Method: " << req.method << std::endl;
            //     std::cout << "  Path:   " << req.path << std::endl;
            //     std::cout << "  Version: " << req.version << std::endl;

            //     // 打印响应日志
            //     std::cout << "Response:" << std::endl;
            //     std::cout << "  Status: " << res.status << std::endl;
            //     std::cout << "  Version: " << res.version << std::endl;
            // };

            std::string download_url = _download_prefix + "(.*)"; // 避免下载前缀路径变化
            _server.Get(download_url, Download);                  // 匹配任意字符任意次

            // 其他没有协商的请求
            //_server.Get("/(.*)", No);

            _server.listen(_server_ip.c_str(), _server_port);

            // //Debug
            // printf("Server starting on %s:%d...\n", _server_ip.c_str(), _server_port);
            // if (_server.listen(_server_ip.c_str(), _server_port)) {
            //     printf("Server started successfully!\n");
            //     //do Something
            // } else {
            //     printf("Failed to start server!\n");
            // }
            // DEBUG printf("------------RunModule运行结束------------\n");

            return true;
        }
    };
}
