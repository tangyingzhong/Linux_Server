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
	m_EventTable.clear();

	std::vector<epoll_event>().swap(m_EventTable);
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
					continue;
				}

				char RevData[500] = {0};

				int iRevSize = 0;

				if (!Receive(iCurFd, RevData,500, iRevSize))
				{
					close(iCurFd);

					continue;
				}

				// This is an event changed status so read nothing
				if (iRevSize==0)
				{
					continue;
				}

				std::cout << "Read from client: " 
					<<std::to_string(iCurFd)
					<<"--"
					<< RevData 
					<< std::endl;

				m_EpollEvent.data.fd = iCurFd;

				m_EpollEvent.events = EPOLLOUT | EPOLLET;

				// Change epoll event to listen output event
				int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_MOD, iCurFd, &m_EpollEvent);
				if (iEpollRegRet == -1)
				{
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
					continue;
				}

				std::string strSendText = "Server sends text to ";

				strSendText = strSendText + std::to_string(iCurFd);

				char* pData = const_cast<char*>(strSendText.c_str());

				int iTotalLen = static_cast<int>(strSendText.length());

				int iLeftLen = iTotalLen;

				int iBlockLen = 1024;

				if (iLeftLen < iBlockLen)
				{
					iBlockLen = iLeftLen;
				}

				int iSize = 0;

				if (!Send(iCurFd, pData, iBlockLen, iSize))
				{
					close(iCurFd);

					continue;
				}
	
				m_EpollEvent.data.fd = iCurFd;

				m_EpollEvent.events = EPOLLIN | EPOLLET;

				// Change epoll event to listen input event
				int iEpollRegRet = epoll_ctl(GetEpollfd(), EPOLL_CTL_MOD, iCurFd, &m_EpollEvent);
				if (iEpollRegRet == -1)
				{
					char* pErrorMsg = strerror(errno);

					SetErrorText(pErrorMsg);

					return false;
				}
			}
		}
	}

	close(GetListenSocket());

	close(GetEpollfd());

	return false;
}

// Stop the server
bool ParallelServer::Stop()
{
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

				continue;
			}
			else
			{
				char* pErrorMsg = strerror(errno);

				SetErrorText(pErrorMsg);

				return false;
			}
		}
		else if (lWrittenSize==0)
		{
			break;
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

				continue;
			}
			else
			{
				char* pErrorMsg = strerror(errno);

				SetErrorText(pErrorMsg);

				return false;
			}
		}
		else if (lReadSize==0)
		{
			break;
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