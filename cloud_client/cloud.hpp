#pragma once
#include<sstream>
#include"data.hpp"
#include"httplib.h"
#include<windows.h>

namespace cloud
{
#define SERVER_IP "114.55.52.91"
#define SERVER_PORT 8101
	class Backup
	{
	private:
		std::string _back_dir;//备份文件目录
		DataManager* _data;
	public:
		//cloud::Backup bp(./code/back,./backfile.dat);
		Backup(const std::string& back_dir, const std::string& back_file)
			:_back_dir(back_dir)
		{
			_data = new DataManager(back_file);
		}
		//获取文件的唯一标识
		std::string GetFileIdentifier(const std::string &filename)
		{
			//a.txt-fsize-mtime
			FileUtil fu(filename);
			std::stringstream ss;
			ss << fu.FileName() << "-" << fu.FileSize() << "-" << fu.LastMTime();
			return ss.str();
		}
		//文件上传
		bool Upload(const std::string& filename)
		{
			//获取文件数据
			FileUtil fu(filename);
			std::string body;
			fu.GetContent(&body);
			//搭建http客户端上传文件数据
			httplib::Client client(SERVER_IP, SERVER_PORT);
			httplib::MultipartFormData item;
			item.content = body;
			item.filename = fu.FileName();
			item.content_type = "application/octet-stream";
			item.name = "file";
			httplib::MultipartFormDataItems items;
			items.push_back(item);
			auto res = client.Post("/upload", items);
			if (res->status != 200 || !res)
			{
				return false;
			}
			return true;
		}

		bool IsNeedUpload(const std::string &filename)
		{
			//需要上传的文件的判断条件：文件是新增的，不是新增但是被修改过
			//文件是新增的：看一下有没有历史备份信息
			//不是新增但是被修改过：有历史消息，但是历史的唯一标识与当前最新的唯一标识不一致
			std::string id;
			if (_data->GetOneByKey(filename, &id) != false)
			{
				//说明不是新增的
				//接下来判断唯一标识是否一致
				std::string new_id = GetFileIdentifier(filename);
				if (new_id == id)
				{
					return false;//不需要上传-上次上传之后没有被修改过
				}
			}
			//是新增的
			// |
			// V
			//如果一个文件比较大，正在缓慢的拷贝到这个目录下，拷贝需要一个过程
			//如果每次遍历则都会判断表示不一致需要上传，一个几十G的文件会被上传很多次
			//因此应该判断一个文件一段时间都没有被修改过，才能上传
			FileUtil fu(filename);
			if (time(NULL) - fu.LastMTime() < 3)//3秒钟内刚修改过———认为文件还在修改中
			{
				return false;
			}
			std::cout << filename << "need upload!\n";
			return true;
		}

		bool RunModule()
		{
			while (1)
			{
				//1.遍历获取指定文件夹中的所有文件
				FileUtil fu(_back_dir);
				std::vector<std::string> arry;
				fu.ScanDirectory(&arry);
				//2.逐个判断文件是否需要上传
				for (auto& a : arry)
				{
					if (IsNeedUpload(a) == false)
					{
						continue;
					}
					//3.如果需要上传文件则上传文件
					//如果成功了就新增备份信息，如果失败就不更新，下次上传时会重新上传
					if (Upload(a) == true)
					{
						_data->Insert(a, GetFileIdentifier(a));//新增文件备份信息
						std::cout << a << "upload success!\n";
					}
				}
				Sleep(1);
			}
		}
	};
}