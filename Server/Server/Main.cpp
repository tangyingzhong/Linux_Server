#include <string>
#include "IParallelServer.h"
#include "ParallelServer.h"

int main(int args, char** argv)
{
	std::string strListenNum = "1000000";

	if (args > 1)
	{
		strListenNum = argv[1];
	}

	IParallelServer* pServer = new ParallelServer();
	if (pServer==nullptr)
	{
		return -1;
	}

	int iListenNum = std::stoi(strListenNum);

	if (!pServer->Configure(8888, iListenNum))
	{
		std::cout << pServer->GetErrorMsg() << std::endl;

		return -1;
	}

	if (!pServer->Start())
	{
		std::cout << pServer->GetErrorMsg() << std::endl;

		return -1;
	}

	pServer->Stop();

	delete pServer;

	pServer = nullptr;

	return 0;
}
