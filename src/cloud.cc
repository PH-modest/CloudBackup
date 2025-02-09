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

void JsonUtilTest()
{
    char *name = "小明";
    int age = 19;
    float score[] = {85,88.5,99};
    Json::Value root;
    root["姓名"] = name;
    root["年龄"] = age;
    root["成绩"].append(score[0]);
    root["成绩"].append(score[1]);
    root["成绩"].append(score[2]);
    std::string json_str;
    cloud::JsonUtil::Serialize(root,&json_str);
    std::cout<<json_str<<std::endl;

    Json::Value val;
    cloud::JsonUtil::UnSerialize(json_str,&val);
    std::cout<<val["姓名"].asString()<<std::endl;
    std::cout<<val["年龄"].asInt()<<std::endl;
    for(int i=0;i<val["成绩"].size();i++)
    {
        std::cout<<val["成绩"][i].asFloat()<<std::endl;
    }
}

int main(int argc,char *argv[])
{
    //FileUtilTest(argv[1]);
    JsonUtilTest();
    return 0;
}