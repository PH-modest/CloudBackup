#include "util.hpp"

void FileUtilTest(const std::string &filename)
{
/*
    //属性获取接口测试代码
    cloud::FileUtil fu(filename);
    std::cout<<fu.FileName()<<std::endl;
    std::cout<<fu.FileSize()<<std::endl;
    std::cout<<fu.LastATime()<<std::endl;
    std::cout<<fu.LastMTime()<<std::endl;
*/    
    
/*
    //读写文件接口测试代码
    cloud::FileUtil fu(filename);
    std::string body;
    fu.GetContent(&body);

    cloud::FileUtil newfu("./hello.txt");
    newfu.SetContent(body);
    return;
*/

/*
    //压缩与解压缩
    std::string packname = filename + ".lz";
    cloud::FileUtil fu(filename);
    fu.Compress(packname);
    cloud::FileUtil pfu(packname);
    pfu.UnCompress("./unpack.txt");
    return;
*/  

    //目录操作
    cloud::FileUtil fu(filename);
    fu.CreateDirectory();
    std::vector<std::string>arry;
    fu.ScanDirectory(&arry);
    for(auto& p : arry)
    {
        std::cout<<p<<std::endl;
    }
    return;
}

int main(int argc,char *argv[])
{
    FileUtilTest(argv[1]);
    return 0;
}