#include "httplib.h"

#define SERVER_IP "114.55.52.91"
#define SERVER_PORT 8120
int main()
{
    httplib::Client client(SERVER_IP,SERVER_PORT);//实例化client对象，用于搭建客户端

    httplib::MultipartFormData item;
    item.name = "file";
    item.filename = "hello.txt";
    item.content = "Hello World!";//上传文件时，这里给的就是文件内容
    item.content_type = "text/plain";

    httplib::MultipartFormDataItems items;
    items.push_back(item);

    auto res = client.Post("/multipart",items);
    std::cout<<res->status<<std::endl;
    std::cout<<res->body<<std::endl;
    return 0;
}