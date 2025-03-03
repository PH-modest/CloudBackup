#pragma once
#include<iostream>
#include<vector>
#include<string>
#include<fstream>
#include<sys/stat.h>
#include<filesystem>
//#include<experimental/filesystem>

namespace cloud
{
    namespace fs = std::filesystem;
    class FileUtil
    {
    private:
        std::string _filename;
    public:
        FileUtil(const std::string& FileName)
            :_filename(FileName)
        {}
        //删除文件
        bool Remove()
        {
            if (this->Exists() == false)
            {
                return true;
            }
            remove(_filename.c_str());
            return true;
        }
        //获取文件大小
        int64_t FileSize()
        {
            struct stat st;
            if (stat(_filename.c_str(), &st) < 0)
            {
                //std::cout<<"Get file size failed!\n";
                std::cout << "Get file size failed,errno: " << strerror(errno) << std::endl;
                return 0;
            }
            return st.st_size;
        }
        //获取文件最后一次修改时间
        time_t LastMTime()
        {
            struct stat st;
            if (stat(_filename.c_str(), &st) < 0)
            {
                std::cout << "get file size failed!,errno:" << strerror(errno) << std::endl;
                return -1;
            }
            return st.st_mtime;
        }
        //获取文件最后一次访问时间
        time_t LastATime()
        {
            struct stat st;
            if (stat(_filename.c_str(), &st) < 0)
            {
                std::cout << "Get file size failed,errno:" << strerror(errno) << std::endl;
                return -1;
            }
            return st.st_atime;
        }
        //获取文件路径名中的文件名称
        std::string FileName()
        {
            //./abc/test.txt
            //size_t pos = _filename.find_last_of("/");
            size_t pos = _filename.find_last_of("\\");
            if (pos == std::string::npos)
            {
                return _filename;
            }
            //return fs::path(_filename).filename().string();
            return _filename.substr(pos + 1);
        }
        //获取文件指定位置，指定长度的数据
        bool GetPosLen(std::string* body, size_t pos, size_t len)
        {
            //首先判断读取的长度是否会超出文件大小
            size_t fsize = this->FileSize();
            if (pos + len > fsize)
            {
                std::cout << "get file len is error\n";
                return false;
            }
            //获取文件数据
            std::ifstream ifs;
            ifs.open(_filename, std::ios::binary);//以二进制形式打开文件
            if (ifs.is_open() == false)//判断是否打开成功
            {
                std::cout << "read open file failed!\n";
                return false;
            }
            ifs.seekg(pos, std::ios::beg);//从起始位置偏移pos单位
            body->resize(len);//设置body大小
            ifs.read(&(*body)[0], len);//读取len个长度的字符到body中
            if (ifs.good() == false)//检查文件是否有问题
            {
                std::cout << "get file content failed\n";
                ifs.close();//关闭文件
                return false;
            }
            ifs.close();
            return true;
        }
        //获取文件数据
        bool GetContent(std::string* body)
        {
            size_t fsize = this->FileSize();
            return GetPosLen(body, 0, fsize);
        }
        //向文件写入数据
        bool SetContent(const std::string& body)
        {
            std::ofstream ofs;
            ofs.open(_filename, std::ios::binary);
            if (ofs.is_open() == false)
            {
                std::cout << "write open file failed\n";
                //ofs.close();
                return false;
            }
            ofs.write(&body[0], body.size());
            if (ofs.good() == false)
            {
                std::cout << "write file content failed!\n";
                ofs.close();
                return false;
            }
            ofs.close();
            return true;
        }
        //判断文件是否存在
        bool Exists()
        {
            return fs::exists(_filename);
        }
        //创建目录
        bool CreateDirectory()
        {
            if (this->Exists() == true)
            {
                return true;
            }
            return fs::create_directories(_filename);
        }
        //浏览获取目录下所有文件路径名
        bool ScanDirectory(std::vector<std::string>* arry)
        {
            this->CreateDirectory();
            for (auto& p : fs::directory_iterator(_filename))
            {
                if (fs::is_directory(p) == true)//判断当前文件名是否是目录
                {
                    continue;
                }
                //relative_path：带有路径的文件名
                //用path实例化p，然后以string类型返回相对路径
                arry->push_back(fs::path(p).relative_path().string());
            }
            return true;
        }
    };
}
