#include <string.h>
#include "IParallelServer.h"
#include "ParallelServer.h"

int main()
{
	IParallelServer* pServer = new ParallelServer();
	if (pServer==nullptr)
	{
		return -1;
	}

	if (!pServer->Configure(8888,1000))
	{
		std::cout << pServer->GetErrorMsg() << std::endl;

		return -1;
	}

	if (!pServer->Start())
	{
		std::cout << pServer->GetErrorMsg() << std::endl;

		return -1;
	}

	delete pServer;

	pServer = nullptr;

	return 0;
}
