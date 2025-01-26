#include <iostream>
#include <sstream>
#include <string>
#include <memory.h>
#include <jsoncpp/json/json.h>

int main()
{
    const char *name = "小明";
    int age = 18;
    float score[] = {77.5,88,66.9};

    Json::Value root;
    root["姓名"] = name;
    root["年龄"] = age;
    root["成绩"].append(score[0]);
    root["成绩"].append(score[1]);
    root["成绩"].append(score[2]);

    Json::StreamWriterBuilder swb;
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
    std::stringstream ss;
    sw->write(root,&ss);
    std::cout<<ss.str()<<std::endl;
    return 0;
}

