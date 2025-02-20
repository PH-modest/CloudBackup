#pragma once
#include "config.hpp"
#include "data.hpp"
#include <unistd.h>

extern cloud::DataManager *_data;

namespace cloud
{
    class HotManager
    {
    private:
        std::string _back_dir;//备份文件路径
        std::string _pack_dir;//压缩后文件路径
        std::string _pack_suffix;//压缩的文件后缀
        int _hot_time;//热点时间
    private:
        //判断文件是否是热点文件,如果是非热点文件 -> 返回true
        bool HotJudge(std::string &filename)
        {
            FileUtil fu(filename);
            time_t last_atime = fu.LastATime();
            time_t cur_time = time(nullptr);
            if(cur_time - last_atime > _hot_time)
            {
                //大于热点时间，说明是非热点文件，需要被压缩
                return true;
            }
            return false;
        }
    public:
        //初始化
        HotManager()
        {
            //成员变量都可以在config配置文件内获取
            Config *conf = Config::GetInstance();
            _back_dir = conf->GetBackDir();
            _pack_dir = conf->GetPackDir();
            _pack_suffix = conf->GetPackFileSuffix();
            _hot_time = conf->GetHotTime();
            //如果不存在路径，我们需要创建它
            FileUtil dir1(_back_dir);
            FileUtil dir2(_pack_dir);
            dir1.CreateDirectory();
            dir2.CreateDirectory();
        }
        //热点管理
        bool RunModule()
        {
            while(1)
            {
                //1.获取路径下所有的文件
                std::vector<std::string> arry;
                FileUtil fu(_back_dir);
                fu.ScanDirectory(&arry);
                //2.判断是否是热点文件
                for(auto &a : arry)
                {
                    if(HotJudge(a) == false)
                    {
                        //非热点文件
                        continue;
                    }
                    //说明是热点文件
                    //3.获取备份文件信息
                    BackupInfo info;
                    if(_data->GetOneByRealPath(a,&info)==false)
                    {
                        //如果获取不到，说明没有被创建，那么我们就直接创建一下
                        info.NewBackupInfo(a);
                    }
                    //4.压缩非热点的备份文件
                    FileUtil tmp(a);
                    tmp.Compress(info.pack_path);//???
                    //5.压缩结束之后，删除源文件
                    tmp.Remove();
                    //6.修改备份文件
                    info.pack_flag=true;
                    _data->Update(info);
                }
                usleep(1000);
            }
            return true;
        }
    };
}
