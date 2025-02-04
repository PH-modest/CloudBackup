#include<iostream>
#include<vector>
#include<string>
#include<fstream>
#include<sys/stat.h>

namespace cloud
{
    class FileUtil
    {
    private:
        std::string _filename;
    public:
        FileUtil(const std::string &FileName)
        :_filename(FileName)
        {}
        //获取文件大小
        int64_t FileSize()
        {
            struct stat st;
            if(stat(_filename.c_str(),&st) < 0)
            {
                std::cout<<"Get file size failed!\n";
                return -1;
            }
            return st.st_size;
        }
        //获取文件最后一次修改时间
        time_t LastMTime()
        {
            struct stat st;
            if(stat(_filename.c_str(),&st) < 0)
            {
                std::cout<<"Get file size failed!\n";
                return -1;
            }
            return st.st_mtime;
        }
        //获取文件最后一次访问时间
        time_t LastATime()
        {
            struct stat st;
            if(stat(_filename.c_str(),&st) < 0)
            {
                std::cout<<"Get file size failed!\n";
                return -1;
            }
            return st.st_atime;
        }
        //获取文件路径名中的文件名称
        std::string FileName()
        {
            //./abc/test.txt
            size_t pos = _filename.find_last_of("/");
            if(pos==std::string::npos)
            {
                return _filename;
            }
            else
            {
                return _filename.substr(pos+1);
            }
        }
    };
}