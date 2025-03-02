#pragma once
#include"util.hpp"
#include<sstream>
#include<unordered_map>

namespace cloud
{
	class DataManager
	{
	private:
		std::string _backup_file;//备份信息的持久化存储文件
		std::unordered_map<std::string, std::string> _table;
	public:
		DataManager(const std::string& backup_file):_backup_file(backup_file) 
		{
			InitLoad();//启动时自动初始化加载
		}
		bool Storage() 
		{
			//1.获取所有的备份信息
			std::stringstream ss;
			for (auto it = _table.begin(); it != _table.end(); ++it)
			{
				//2.将所有的信息进行指定持久化格式组织
				ss << it->first << " " << it->second << "\n";
			}
			//3.持久化存储
			FileUtil fu(_backup_file);
			fu.SetContent(ss.str());
			return true;
		}

		int Split(const std::string& str, const std::string& sep, std::vector<std::string>* arry)
		{
			int count = 0;
			size_t pos = 0, idx = 0;
			while (1)
			{
				pos = str.find(sep, idx);
				if (pos == std::string::npos)
				{
					break;
				}
				if (pos == idx)
				{
					idx = pos + sep.size();
					continue;
				}
				//substr(截取起始位置，长度)
				std::string tmp = str.substr(idx, pos - idx);
				arry->push_back(tmp);
				count++;
				idx = pos + sep.size();
			}
			if (idx < str.size())
			{
				arry->push_back(str.substr(idx));
				count++;
			}
			return count;
		}

		bool InitLoad() 
		{
			//1.从文件中读取所有数据
			FileUtil fu(_backup_file);
			std::string body;
			fu.GetContent(&body);
			//2.进行数据解析，添加到表中
			std::vector<std::string> arry;
			Split(body, "\n", &arry);
			for (auto& a : arry)
			{
				std::vector<std::string> tmp;
				int cnt = Split(a, " ", &tmp);
				if (cnt != 2)
				{
					continue;
				}
				_table[tmp[0]] = tmp[1];
			}
			return true;
		}
		bool Insert(const std::string& key, const std::string& val) 
		{
			_table[key] = val;
			Storage();
			return true;
		}
		bool Updata(const std::string& key, const std::string& val) 
		{
			_table[key] = val;
			Storage();
			return true;
		}
		bool GetOneByKey(const std::string& key, std::string* val) 
		{
			auto it = _table.find(key);
			if (it == _table.end())
			{
				return false;
			}
			*val = it->second;
			return true;
		}
		~DataManager() {}
	};
}