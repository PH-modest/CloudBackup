#include<iostream>
#include<string>
#include<memory>
#include<jsoncpp/json/json.h>

int main()
{
    std::string str = R"({"姓名":"小黑","年龄":19,"成绩":[58.5,60,36.5]})";//需要被反序列化的数据
    Json::Value root;//用来存放反序列化之后的数据
    Json::CharReaderBuilder crb;//用来生成Json::CharReader对象
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader());//指向Json::CharReader对象，使其调用parse函数
    std::string error;//获取出错信息
    bool ret = cr->parse(str.c_str(),str.c_str()+str.size(),&root,&error);//将反序列化之后的数据存储在root中
    if(ret==false)
    {
        std::cout<<"parse error:"<<error<<std::endl;
        return -1;
    }
    std::cout<<root["姓名"].asString()<<std::endl;
    std::cout<<root["年龄"].asInt()<<std::endl;
    int sz = root["成绩"].size();
    for(int i=0;i<sz;i++)
    {
        std::cout<<root["成绩"][i]<<std::endl;
    }
    return 0;
}
