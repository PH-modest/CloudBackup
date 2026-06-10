#pragma once
#include "config.hpp"
#include "data.hpp"
#include <unistd.h>
#include <experimental/filesystem>

extern cloud::DataManager *_data;

namespace cloud
{
    namespace fs = std::experimental::filesystem;

    class HotManager
    {
    private:
        std::string _back_dir;    // 备份文件路径
        std::string _pack_dir;    // 压缩后文件路径
        std::string _pack_suffix; // 压缩的文件后缀
        int _hot_time;            // 热点时间
    private:
        // 判断文件是否是热点文件,如果是非热点文件 -> 返回true
        bool HotJudge(const std::string &filename)
        {
            FileUtil fu(filename);
            time_t last_atime = fu.LastATime();
            time_t cur_time = time(nullptr);
            if (cur_time - last_atime > _hot_time)
            {
                // 大于热点时间，说明是非热点文件，需要被压缩
                return true;
            }
            return false;
        }

    public:
        HotManager()
        {
            Config *conf = Config::GetInstance();
            _back_dir = conf->GetBackDir();
            _pack_dir = conf->GetPackDir();
            _pack_suffix = conf->GetPackFileSuffix();
            _hot_time = conf->GetHotTime();
            FileUtil dir1(_back_dir);
            FileUtil dir2(_pack_dir);
            dir1.CreateDirectory();
            dir2.CreateDirectory();

            std::string private_prefix = conf->GetPrivateDirPrefix();

            // 创建公共目录
            FileUtil public_back(_back_dir + "public/");
            FileUtil public_pack(_pack_dir + "public/");
            public_back.CreateDirectory();
            public_pack.CreateDirectory();

            // 创建私有基目录（用户子目录在上传时动态创建）
            FileUtil private_back_base(_back_dir + private_prefix);
            FileUtil private_pack_base(_pack_dir + private_prefix);
            private_back_base.CreateDirectory();
            private_pack_base.CreateDirectory();
        }
        // 热点管理
        bool RunModule()
        {
            while (1)
            {
                std::vector<std::string> arry;
                // 扫描公共备份目录
                FileUtil fu_public_back(_back_dir + "public/");
                if (fu_public_back.Exists())
                {
                    fu_public_back.ScanDirectory(&arry);
                    ProcessFiles(arry, "public/");
                }
                arry.clear();

                // 动态扫描所有私有备份目录（扫描 _back_dir + private_prefix 下的子目录）
                std::string private_prefix = Config::GetInstance()->GetPrivateDirPrefix();
                std::string private_base = _back_dir + private_prefix;
                std::vector<std::string> user_dirs;
                for (auto &p : fs::directory_iterator(private_base))
                {
                    if (fs::is_directory(p))
                    {
                        std::string user_dir = fs::path(p).filename().string() + "/";
                        user_dirs.push_back(user_dir);
                    }
                }
                for (const auto &user_dir : user_dirs)
                {
                    FileUtil fu_private_back(private_base + user_dir);
                    if (fu_private_back.Exists())
                    {
                        fu_private_back.ScanDirectory(&arry);
                        ProcessFiles(arry, private_prefix + user_dir);
                    }
                    arry.clear();
                }

                usleep(1000 * 1000); // 每秒扫描
            }
            return true;
        }

    private:
        void ProcessFiles(std::vector<std::string> &arry, const std::string &subdir)
        {
            for (auto &a : arry)
            {
                if (a.empty() || a == "." || a == "..")
                    continue;
                std::string full_path = _back_dir + subdir + a;
                FileUtil fu_full(full_path);
                if (!fu_full.Exists())
                    continue;

                // 检查文件是否应该被压缩
                BackupInfo info;
                if (!_data->GetOneByRealPath(full_path, &info))
                    continue;
                
                // 如果文件已经被压缩，跳过
                if (info.pack_flag)
                    continue;

                // 判断是否是热点文件（非热点文件需要压缩）
                if (HotJudge(full_path))
                {
                    std::string pack_dir_sub = _pack_dir + subdir;
                    FileUtil fu_pack_dir(pack_dir_sub);
                    fu_pack_dir.CreateDirectory();

                    std::string pack_path = pack_dir_sub + a + _pack_suffix;
                    info.pack_path = pack_path;
                    FileUtil tmp(full_path);
                    if (!tmp.Compress(info.pack_path))
                    {
                        std::cerr << "Compress failed for: " << full_path << std::endl;
                        continue;
                    }
                    if (!tmp.Remove())
                    {
                        std::cerr << "Remove after compress failed: " << full_path << std::endl;
                        // 压缩成功但删除失败，删除压缩文件
                        FileUtil(pack_path).Remove();
                        continue;
                    }
                    info.pack_flag = true;
                    _data->Update(info);
                    std::cerr << "File compressed: " << full_path << " -> " << pack_path << std::endl;
                }
            }
        }
    };
}