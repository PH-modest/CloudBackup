#pragma once
#include "data.hpp"//上传文件需要备份，就需要将文件放入进去
#include "httplib.h"

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
        static void Upload(const httplib::Request &req,httplib::Response &rsp)
        {
            printf("----------Upload-----------\n");
            //post /upload    文件数据在正文中（正文并不全是文件数据）
            //首先判断有没有上传的文件区域
            auto ret = req.has_file("file");
            if(ret == false)
            {
                rsp.status = 400;
                return;
            }
            // DEGUG printf("----------有文件-----------\n");
            //获取数据
            const auto& file = req.get_file_value("file");
            // file.filename//文件名称
            // file.content//文件数据
            std::string back_dir = Config::GetInstance()->GetBackDir();
            std::string realpath = back_dir + FileUtil(file.filename).FileName();//调用FileUtil匿名对象
            FileUtil fu(realpath);
            fu.SetContent(file.content);//将数据写入文件中
            BackupInfo info;
            info.NewBackupInfo(realpath);//组织备份的文件信息
            _data->Insert(info);//向数据管理模块添加备份的文件信息
            // DEBUG printf("----------Upload运行结束-----------\n");
            return;
        }
        static std::string TimetoStr(time_t t)
        {
            std::string tmp = std::ctime(&t);
            return tmp;
        }
        static void ListShow(const httplib::Request &req,httplib::Response &rsp)
        {
            //1. 获取所有的文件备份信息
            std::vector<BackupInfo> arry;
            _data->GetAll(&arry);
            //2. 根据所有备份信息，组织html文件数据
            std::stringstream ss;
            ss<<"<html><head><title>Download</title></head>";
            ss<<"<body><h1>Download</h1><table>";
            for(auto &a : arry)
            {
                ss<<"<tr>";
                std::string filename = FileUtil(a.real_path).FileName();
                ss<<"<td><a href='"<<a.url_path<<"'>"<<filename<<"</a></td>";
                ss<<"<td align='right'>"<<TimetoStr(a.mtime)<<"</td>";
                ss<<"<td align='right'>"<<a.fsize/1024<<"k</td>";
                ss<<"</tr>";
            }
            ss<<"</table></body></html>";
            rsp.body = ss.str();
            rsp.set_header("Content-Type","text/html");
            rsp.status = 200;
            return;
        }
        static void Download(const httplib::Request &req,httplib::Response &rsp){}
    public:
        Service()
        {
            //前三个变量初始化数据可以从配置文件里获取，_server创建时就自动初始化了
            Config *conf = Config::GetInstance();
            _server_port = conf->GetServerPort();
            _server_ip = conf->GetServerIp();
            _download_prefix = conf->GetDownloadPrefix();

        }
        bool RunModule()
        {
            //创建映射关系
            //DEBUG printf("-----------RunModule-------------\n");
            _server.Post("/upload",Upload);
            //DEBUG printf("-----------Post upload-------------\n");
            _server.Get("/listshow",ListShow);
            _server.Get("/",ListShow);
            std::string download_url = _download_prefix + "(.*)";//避免下载前缀路径变化
            _server.Get(download_url,Download);//匹配任意字符任意次
            _server.listen(_server_ip.c_str(),_server_port);
            // //Debug
            // printf("Server starting on %s:%d...\n", _server_ip.c_str(), _server_port);
            // if (_server.listen(_server_ip.c_str(), _server_port)) {
            //     printf("Server started successfully!\n");
            //     //do Something
            // } else {
            //     printf("Failed to start server!\n");
            // }
            //DEBUG printf("------------RunModule运行结束------------\n");
            return true;
        }
    };
}
