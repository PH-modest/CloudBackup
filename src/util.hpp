#include<iostream>
#include<vector>
#include<string>
#include<fstream>
#include<sys/stat.h>
#include<experimental/filesystem>
#include<jsoncpp/json/json.h>
#include"bundle.h"

namespace cloud
{
    namespace fs = std::experimental::filesystem;
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
        //获取文件指定位置，指定长度的数据
        bool GetPosLen(std::string *body,size_t pos,size_t len)
        {
            //首先判断读取的长度是否会超出文件大小
            size_t fsize = this->FileSize();
            if(pos+len > fsize)
            {
                std::cout<<"get file len is error\n";
                return false;
            }
            //获取文件数据
            std::ifstream ifs;
            ifs.open(_filename,std::ios::binary);//以二进制形式打开文件
            if(ifs.is_open() == false)//判断是否打开成功
            {
                std::cout<<"read open file failed!\n";
                return false;
            }
            ifs.seekg(pos,std::ios::beg);//从起始位置偏移pos单位
            body->resize(len);//设置body大小
            ifs.read(&(*body)[0],len);//读取len个长度的字符到body中
            if(ifs.good() == false)//检查文件是否有问题
            {
                std::cout<<"get file content failed\n";
                ifs.close();//关闭文件
                return false;
            }
            ifs.close();
            return true;
        }
        //获取文件数据
        bool GetContent(std::string *body)
        {
            size_t fsize = this->FileSize();
            return GetPosLen(body,0,fsize);
        }
        //向文件写入数据
        bool SetContent(const std::string &body)
        {
            std::ofstream ofs;
            ofs.open(_filename,std::ios::binary);
            if(ofs.is_open()==false)
            {
                std::cout<<"write open file failed\n";
                //ofs.close();
                return false;
            }
            ofs.write(&body[0],body.size());
            if(ofs.good()==false)
            {
                std::cout<<"write file content failed!\n";
                ofs.close();
                return false;
            }
            ofs.close();
            return true;
        }
        //压缩
        bool Compress(const std::string &packname)
        {
            //获取源文件内容
            std::string body;
            if(this->GetContent(&body)==false)
            {
                std::cout<<"Compress get file content failed!\n";
                return false;
            }
            //对数据进行压缩
            std::string packed = bundle::pack(bundle::LZIP,body);
            //将压缩后的数据存储到压缩包文件中
            FileUtil fu(packname);
            if(fu.SetContent(packed) == false)
            {
                std::cout<<"compress write packed data failed!\n";
                return false;
            }
            return true;
        }
        //解压缩
        bool UnCompress(const std::string &filename)
        {
            //获取压缩包内数据
            std::string body;
            if(this->GetContent(&body) == false)
            {
                std::cout<<"uncompress get file content failed!\n";
                return false;
            }
            //将压缩数据进行解压缩
            std::string unpacked = bundle::unpack(body);
            //将解压缩后的数据写入到新文件
            FileUtil fu(filename);
            if(fu.SetContent(unpacked) == false)
            {
                std::cout<<"uncompress write unpacked data failed!\n";
                return false;
            }
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
            if(this->Exists()==true)
            {
                return true;
            }
            return fs::create_directories(_filename);
        }
        //浏览获取目录下所有文件路径名
        bool ScanDirectory(std::vector<std::string> *arry)
        {
            for(auto& p : fs::directory_iterator(_filename))
            {
                if(fs::is_directory(p)==true)//判断当前文件名是否是目录
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

    class JsonUtil
    {
    public:
        //序列化
        static bool Serialize(const Json::Value &root,std::string *str)
        {
            Json::StreamWriterBuilder swb;//初始化一个用于配置JSON序列化参数的构建器
            swb.settings_["emitUTF8"] = true;//采用UTF-8的编码形式，默认是Unicode
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());//生成一个JSON写入器，用智能指针管理生命周期
            std::stringstream ss;
            //将root中的JSON数据写入字符串流ss
            if(sw->write(root,&ss) != 0)
            {
                std::cout<<"json write failed!\n";
                return false;
            }
            *str = ss.str();//从字符串ss中提取内容，赋值给输出参数str指向的字符串
            return true;
        }
        //反序列化
        static bool UnSerialize(const std::string &str,Json::Value *root)
        {
            Json::CharReaderBuilder crb;//用来生成Json::CharReader对象
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());//指向Json::CharReader对象，使其调用parse函数
            std::string error;//获取出错信息
            bool ret = cr->parse(str.c_str(),str.c_str()+str.size(),root,&error);//将反序列化之后的数据存储在root中
            if(ret==false)
            {
                std::cout<<"parse error:"<<error<<std::endl;
                return false;
            }
            return true;
        }
    };
}