#include <string.h>
#include "Interface/IClient.h"
#include "Network/Client.h"
#include "Interface/IServer.h"
#include "Network/Server.h"
#include "Interface/IParallelServer.h"
#include "Network/ParallelServer.h"

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


//int main()
//{
//	IServer* pServer = new Server();
//
//	pServer->Configure(8888, 20);
//
//	if (!pServer->Start())
//	{
//		std::cout << pServer->GetErrorMsg() << std::endl;
//
//		return -1;
//	}
//
//	std::cout << "Get a client entry"  << std::endl;
//
//	// Send something to the client
//	std::string TempData= "Hello,client,how are you ?";
//
//	if (!pServer->Send(TempData.c_str(), static_cast<int>(TempData.length())))
//	{
//		std::cout << pServer->GetErrorMsg() << std::endl;
//
//		return -1;
//	}
//
//	// Wait for a respond from client
//	char RevData[500] = {0};
//
//	if (!pServer->Receive(RevData, 500))
//	{
//		std::cout << pServer->GetErrorMsg() << std::endl;
//
//		return -1;
//	}
//
//	std::cout << "Rev client's data:" << RevData << std::endl;
//
//	pServer->Stop();
//
//	delete pServer;
//
//	pServer = nullptr;
//
//	return 0;
//}

