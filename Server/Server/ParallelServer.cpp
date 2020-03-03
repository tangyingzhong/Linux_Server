#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "ParallelServer.h"

// Construct the ParallelServer
ParallelServer::ParallelServer():m_iEpollfd(0),
m_iListenSocket(-1),
m_iServerPortNo(0),
m_iListenNum(1),
m_strErrorText(""),
m_bDisposed(false)
{
	memset(&m_EpollEvent, 0, sizeof(m_EpollEvent));
}

// Detructe the ParallelServer
ParallelServer::~ParallelServer()
{
	Destory();
}

// Destory the server
void ParallelServer::Destory()
{
	Stop();
}

// Configure the server
bool ParallelServer::Configure(int iServerPortNo, int iListenNum)
{
	if (iServerPortNo <= 0)
	{
		SetErrorText("Server portNo must >0 !");

		return false;
	}

	if (iListenNum <= 0)
	{
		SetErrorText("Listen number must >0 !");

		return false;
	}

	SetServerPortNo(iServerPortNo);

	SetListenNum(iListenNum);

	m_EventTable.resize(GetListenNum());

	return true;
}

// Prepare the environment
bool ParallelServer::PrepareEnvironment()
{
	// Create listen socket
	int iListenSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (iListenSocket < 0)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	// Set listen socket
	SetListenSocket(iListenSocket);

	SetNonBlockStatus(GetListenSocket());

	// Create epoll fd
	int iEpollFd = epoll_create(GetListenNum() + 1);
	if (iEpollFd==-1)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	SetEpollfd(iEpollFd);

	// Set epoll events
	m_EpollEvent.data.fd = GetListenSocket();

	m_EpollEvent.events = EPOLLIN | EPOLLET;
	
	// Register epoll event
	int iEpollRegRet= epoll_ctl(GetEpollfd(), EPOLL_CTL_ADD, GetListenSocket(), &m_EpollEvent);
	if (iEpollRegRet==-1)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	// Set socket info
	struct sockaddr_in ServerAddr;

	memset(&ServerAddr, 0, sizeof(ServerAddr));

	ServerAddr.sin_family = AF_INET;

	ServerAddr.sin_port = htons(static_cast<uint16_t>(GetServerPortNo()));

	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Set listen socket property
	int on = 1;

	int iRet = setsockopt(GetListenSocket(), SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (iRet < 0)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	// Bind the socket
	int iBindRet = bind(GetListenSocket(), (struct sockaddr*)(&ServerAddr), sizeof(ServerAddr));
	if (iBindRet < 0)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	// Listen count
	if (listen(GetListenSocket(), GetListenNum()) < 0)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return false;
	}

	return true;
}

// Set socket non-block
void ParallelServer::SetNonBlockStatus(int iSock)
{
	if (iSock<0)
	{
		return;
	}

	if (fcntl(iSock, F_SETFL, O_NONBLOCK) < 0)
	{
		char* pErrorMsg = strerror(errno);

		SetErrorText(pErrorMsg);

		return;
	}
}

// Start the server
bool ParallelServer::Start()
{
	// Prepare the environment
	if (!PrepareEnvironment())
	{
		return false;
	}

	while (1)
	{
		int iFdCount = epoll_wait(GetEpollfd(), m_EventTable.data(), GetListenNum(), -1);

		for (int iIndex=0;iIndex<iFdCount;++iIndex)
		{
			int iCurFd = m_EventTable[iIndex].data.fd;
			if (iCurFd==-1)
			{
				break;
			}

			if ((m_EventTable[iIndex].events & EPOLLERR)
				|| (m_EventTable[iIndex].events & EPOLLHUP))
			{
				close(iCurFd);

				m_ClientSet.erase(iCurFd);

				std::cout << "Read from client: "
					<< std::to_string(iCurFd)
					<< "--"
					<< "I am deleted"
					<< std::endl;

				continue;
			}

			if (iCurFd==GetListenSocket())
			{
				// Accept the clients
				struct sockaddr_in PeerAddr;

				socklen_t PeerLen = sizeof(PeerAddr);

				int clientSock = 0;

				while ((clientSock = accept(GetListenSocket(), (struct sockaddr*)(&PeerAddr), &PeerLen)) > 0)
				{
					// Set the client to be non-block
					SetNonBlockStatus(clientSock);

					m_ClientSet.insert(clientSock);

					m_EpollEvent.data.fd = clientSock;

					m_EpollEvent.events = EPOLLIN | EPOLLET;

					// Register epoll event
					int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_ADD, clientSock, &m_EpollEvent);
					if (iEpollRegRet == -1)
					{
						char* pErrorMsg = strerror(errno);

						SetErrorText(pErrorMsg);

						return false;
					}
				}

				continue;
			}
			else if (m_EventTable[iIndex].events & EPOLLIN)
			{
				int iCurFd = m_EventTable[iIndex].data.fd;
				if (iCurFd == -1)
				{
					close(iCurFd);

					m_ClientSet.erase(iCurFd);

					continue;
				}

				char RevData[500] = {0};

				int iRevSize = 0;

				if (!Receive(iCurFd, RevData,500, iRevSize))
				{
					// Client has been closed at this time
					if (iRevSize==0)
					{
						m_ClientSet.erase(iCurFd);

						m_EpollEvent.data.fd = iCurFd;

						m_EpollEvent.events = EPOLLOUT | EPOLLET;

						int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_DEL, iCurFd, &m_EpollEvent);
						if (iEpollRegRet == -1)
						{
							char* pErrorMsg = strerror(errno);

							SetErrorText(pErrorMsg);
						}

						std::cout << "Read from client: "
							<< std::to_string(iCurFd)
							<< "--"
							<< "I am deleted"
							<< std::endl;
					}

					continue;
				}

				std::cout << "Read from client: " 
					<<std::to_string(iCurFd)
					<<"--"
					<< RevData 
					<< std::endl;

				std::string strCondition = RevData;

				std::cout << strCondition << std::endl;

				if (strCondition == "Exit")
				{
					std::cout << "Exit the loop function now" << std::endl;

					return true;
				}

				m_EpollEvent.data.fd = iCurFd;

				m_EpollEvent.events = EPOLLOUT | EPOLLET;

				// Change epoll event to listen output event
				int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_MOD, iCurFd, &m_EpollEvent);
				if (iEpollRegRet == -1)
				{
					close(iCurFd);

					m_ClientSet.erase(iCurFd);

					char* pErrorMsg = strerror(errno);

					SetErrorText(pErrorMsg);

					return false;
				}
			}
			else if (m_EventTable[iIndex].events & EPOLLOUT)
			{
				int iCurFd = m_EventTable[iIndex].data.fd;
				if (iCurFd == -1)
				{
					close(iCurFd);

					m_ClientSet.erase(iCurFd);

					continue;
				}

				std::string strSendText = "I'm fine today,thanks to ";

				strSendText = strSendText + std::to_string(iCurFd);

				int iSize = 0;

				if (!Send(iCurFd, strSendText.c_str(), static_cast<int>(strSendText.length()+1), iSize))
				{
					if (iSize==0)
					{
						m_ClientSet.erase(iCurFd);

						m_EpollEvent.data.fd = iCurFd;

						m_EpollEvent.events = EPOLLIN | EPOLLET;

						int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_DEL, iCurFd, &m_EpollEvent);
						if (iEpollRegRet == -1)
						{
							char* pErrorMsg = strerror(errno);

							SetErrorText(pErrorMsg);
						}
					}

					continue;
				}
	
				m_EpollEvent.data.fd = iCurFd;

				m_EpollEvent.events = EPOLLIN | EPOLLET;

				// Change epoll event to listen input event
				int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_MOD, iCurFd, &m_EpollEvent);
				if (iEpollRegRet == -1)
				{
					close(iCurFd);

					m_ClientSet.erase(iCurFd);

					char* pErrorMsg = strerror(errno);

					SetErrorText(pErrorMsg);

					return false;
				}
			}
		}
	}

	return false;
}

// Cleanup
void ParallelServer::Cleanup()
{
	std::cout << "Start to cleanup resources" << std::endl;
	
	std::cout << "Client number is:" << m_ClientSet.size() <<std::endl;

	for (std::set<int>::iterator Iter=m_ClientSet.begin();
		Iter!=m_ClientSet.end();
		++Iter)
	{
		int iCurFd = *Iter;

		m_EpollEvent.data.fd = iCurFd;

		m_EpollEvent.events = EPOLLIN | EPOLLET;

		int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_DEL, iCurFd, &m_EpollEvent);
		if (iEpollRegRet == -1)
		{
			close(iCurFd);

			char* pErrorMsg = strerror(errno);

			SetErrorText(pErrorMsg);
		}

		close(iCurFd);

		std::cout << "Close the sock fd: "<<iCurFd << std::endl;
	}

	m_ClientSet.clear();

	std::set<int>().swap(m_ClientSet);

	std::cout << "Client table size is:" << m_ClientSet.size() << std::endl;

	m_EventTable.clear();

	std::vector<epoll_event>().swap(m_EventTable);

	std::cout << "Finish the cleanup resources" << std::endl;
}

// Stop the server
bool ParallelServer::Stop()
{
	if (!GetDisposed())
	{
		SetDisposed(true);

		Cleanup();

		close(GetEpollfd());

		SetEpollfd(0);

		close(GetListenSocket());

		SetListenSocket(0);

		std::cout << "Close the epoll sock" << std::endl;

		std::cout << "Close the listen sock" << std::endl;
	}

	return true;
}

// Send the data to server
bool ParallelServer::Send(int iClientSocket, 
	const char* pData,
	int iSendSize,
	int& iRealSendSize)
{
	if (pData == nullptr)
	{
		SetErrorText("Data to be sent is null !");

		return false;
	}

	if (iSendSize <= 0)
	{
		SetErrorText("Inalid data size !");

		return false;
	}

	if (iClientSocket == 0)
	{
		SetErrorText("Inalid client !");

		return false;
	}

	size_t lTotalSize = static_cast<size_t>(iSendSize);

	size_t lLeftSize = lTotalSize;

	size_t lWorkSize = lLeftSize;

	while (lLeftSize>0)
	{
		ssize_t lWrittenSize = write(iClientSocket, pData, lWorkSize);
		if (lWrittenSize == -1)
		{
			if (errno==EAGAIN)
			{
				lWorkSize = 0;

				break;
			}
			else
			{
				close(iClientSocket);

				char* pErrorMsg = strerror(errno);

				SetErrorText(pErrorMsg);

				return false;
			}
		}
		else if (lWrittenSize==0)
		{
			close(iClientSocket);

			return false;
		}

		pData += lWrittenSize;

		lLeftSize -= lWrittenSize;

		lWorkSize = lLeftSize;
	}

	iRealSendSize = static_cast<int>(lTotalSize - lLeftSize);

	return true;
}

// Receive data from the server
bool ParallelServer::Receive(int iClientSocket, 
	char* pData,
	int iRevSize,
	int& iRealRecvSize)
{
	if (pData == nullptr)
	{
		SetErrorText("Data buffer to store receive data is null !");

		return false;
	}

	if (iRevSize <= 0)
	{
		SetErrorText("Inalid size to get data !");

		return false;
	}

	if (iClientSocket == 0)
	{
		SetErrorText("Inalid client !");

		return false;
	}

	size_t lTotalSize = static_cast<size_t>(iRevSize);

	size_t lLeftSize = lTotalSize;

	size_t lWorkSize = lLeftSize;

	while (lLeftSize>0)
	{
		ssize_t lReadSize = read(iClientSocket, pData, lWorkSize);
		if (lReadSize < 0)
		{			
			if (errno==EAGAIN)
			{
				lWorkSize = 0;

				break;
			}
			else
			{
				close(iClientSocket);

				char* pErrorMsg = strerror(errno);

				SetErrorText(pErrorMsg);

				return false;
			}
		}
		else if (lReadSize==0)
		{
			close(iClientSocket);

			return false;
		}

		lLeftSize -= lReadSize;

		lWorkSize = lLeftSize;

		pData += lReadSize;
	}

	iRealRecvSize = static_cast<int>(lTotalSize - lLeftSize);

	return true;
}

// Get the error message 
std::string ParallelServer::GetErrorMsg()
{
	return GetErrorText();
}