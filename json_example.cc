#include <iostream>
#include <sstream>
#include <string>
#include <memory.h>
#include <jsoncpp/json/json.h>

int main()
{
    //需要序列化的数据
    const char *name = "小明";
    int age = 18;
    float score[] = {77.5,88,66.9};

    Json::Value root;
    root["姓名"] = name;
    root["年龄"] = age;
    root["成绩"].append(score[0]);
    root["成绩"].append(score[1]);
    root["成绩"].append(score[2]);

    Json::StreamWriterBuilder swb;//用来创建Json::StreamWriter对象
    swb.settings_["emitUTF8"] = true;//采用UTF-8的编码形式，默认是Unicode
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());//sw智能指针指向Json::StreamWriter对象，从而来调用writer方法
    std::stringstream ss;
    sw->write(root,&ss);//root用来存储反序列化数据，ss用来存储序列化数据
    std::cout<<ss.str()<<std::endl;
    return 0;
}

