#define _CRT_SECURE_NO_WARNINGS 1
#include "util.hpp"
#include "data.hpp"

#define BACKUP_FILE "./backup.dat"

int main()
{
	/*
	//ÃÌº”
	cloud::FileUtil fu("./");
	std::vector<std::string> arry;
	fu.ScanDirectory(&arry);
	cloud::DataManager data(BACKUP_FILE);
	for (auto& a : arry)
	{
		data.Insert(a, "abcdefg");
	}
	*/

	/*
	//≤È—Ø
	cloud::DataManager data(BACKUP_FILE);
	std::string s;
	data.GetOneByKey("./util.hpp",&s);
	std::cout << s << std::endl;
	*/
	

	return 0;
}