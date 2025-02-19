#pragma once
#include "data.hpp"
#include "config.hpp"
#include <unistd.h>

extern cloud::DataManager *_data;

namespace cloud
{
    class HotManager
    {
    private:
        std::string _back_dir;
        std::string _pack_dir;
        std::string _pack_suffix;
        int _hot_time;
    private:
        //判断是否是热点文件,非热点文件返回真，热点文件返回假
        bool HotJudge(std::string &filename)
        {
            FileUtil fu(filename);
            time_t last_atime = fu.LastATime();
            time_t cur_time = time(NULL);
            if(cur_time - last_atime > _hot_time)
            {
                return true;
            }
            return false;
        }
    public:
        HotManager()
        {
            Config *config = Config::GetInstance();
            _back_dir = config->GetBackDir();
            _pack_dir = config->GetPackDir();
            _pack_suffix = config->GetPackFileSuffix();
            _hot_time = config->GetHotTime();
            //不存在时就创建
            FileUtil tmp1(_back_dir);
            FileUtil tmp2(_pack_dir);
            tmp1.CreateDirectory();
            tmp2.CreateDirectory();
        }
        bool RunModule()
        {
            while(1)
            {
                //1.遍历备份目录，获取所有文件名
                FileUtil fu(_back_dir);
                std::vector<std::string> arry;
                fu.ScanDirectory(&arry);
                //2.遍历判断文件是否是非热点文件
                for(auto &a : arry)
                {
                    if(HotJudge(a) == false)
                    {
                        continue;//热点文件不需要特别处理
                    }
                    //获取文件的备份信息
                    BackupInfo info;
                    if(_data->GetOneByRealPath(a,&info)==false)
                    {
                        //现在有一个文件存在，但是没有备份信息
                        info.NewBackupInfo(a);//设置一个新的备份文件信息出来
                    }
                    //3.对非热点文件进行压缩处理
                    FileUtil tmp(a);
                    tmp.Compress(info.pack_path);
                    //4.删除源文件，修改备份文件
                    tmp.Remove();
                    info.pack_flag=true;
                    _data->Update(info);
                }
                usleep(1000);//避免空目录循环遍历，消耗CPU资源过多
            }
            return true;
        }
    };
}
