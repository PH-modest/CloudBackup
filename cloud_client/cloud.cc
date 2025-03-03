#define _CRT_SECURE_NO_WARNINGS 1
#include "util.hpp"
#include "data.hpp"
#include "cloud.hpp"

#define	BACKUP_FILE "./backup.dat"
#define	BACKUP_DIR "./backup/"

void Test_Insert()
{
	//ÃÌº”
	cloud::FileUtil fu("./");
	std::vector<std::string> arry;
	fu.ScanDirectory(&arry);
	cloud::DataManager data(BACKUP_FILE);
	for (auto& a : arry)
	{
		data.Insert(a, "abcdefg");
	}
}

void Test_GetOneByKey()
{
	//≤È—Ø
	cloud::DataManager data(BACKUP_FILE);
	std::string s;
	data.GetOneByKey("./util.hpp", &s);
	std::cout << s << std::endl;
}

void Test_RunModule()
{
	cloud::Backup backup(BACKUP_DIR,BACKUP_FILE);
	backup.RunModule();
}

int main()
{
	//Test_Insert();
	//Test_GetOneByKey();
	Test_RunModule();
	return 0;
}