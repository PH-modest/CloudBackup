#include<iostream>
#include<string>
#include<fstream>
#include"bundle.h"

int main(int argc,char* argv[])
{
    std::cout<<"argv[1] 是原始文件路径名称\n";
    std::cout<<"argv[2] 是压缩包名称\n";
    if(argc<3) return -1;
    std::string ifilename = argv[1];
    std::string ofilename = argv[2];

    std::ifstream ifs;
    ifs.open(ifilename,std::ios::binary);//打开原始文件
    ifs.seekg(0,std::ios::end);//跳转读写位置到文件末尾
    size_t fsize = ifs.tellg();//获取末尾偏移量——文件长度
    ifs.seekg(0,std::ios::beg);//跳转到文件起始位置
    std::string body;
    body.resize(fsize);//调整body文件大小为目标文件大小
    ifs.read(&body[0],fsize);//读取文件所有数据到body

    std::string packed = bundle::pack(bundle::LZIP,body);//以lzip格式压缩文件数据

    std::ofstream ofs;
    ofs.open(ofilename,std::ios::binary);//打开压缩包文件
    ofs.write(&packed[0],packed.size());//将压缩后的文件写入压缩包文件

    ifs.close();
    ofs.close();
    return 0;
}
