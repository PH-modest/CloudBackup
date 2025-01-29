#include<iostream>
#include<fstream>
#include<string>
#include"bundle.h"

int main(int argc,char* argv[])
{
    std::cout<<"argv[1]表示待解压的压缩包\n";
    std::cout<<"argv[2]表示解压后的文件名称\n";
    if(argc<3)
    {
        return -1;
    }
    std::string ifilename = argv[1];
    std::string ofilename = argv[2];

    //打开目标压缩包
    std::ifstream ifs;
    ifs.open(ifilename,std::ios::binary);
    //求文件大小
    ifs.seekg(0,std::ios::end);
    size_t fsize = ifs.tellg();
    ifs.seekg(0,std::ios::beg);
    //将文件内容存放到body中
    std::string body;
    body.resize(fsize);
    ifs.read(&body[0],fsize);
    //关闭文件
    ifs.close();

    //文件解压缩
    std::string unpacked = bundle::unpack(body);

    //将解压缩后的数据写入解压后的文件
    std::ofstream ofs;
    ofs.open(ofilename,std::ios::binary);
    ofs.write(&unpacked[0],unpacked.size());
    ofs.close();
    return 0;
}