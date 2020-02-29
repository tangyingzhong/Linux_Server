///************************************************************************
/// <copyrigth>Voice AI Technology Of ShenZhen</copyrigth>
/// <author>tangyingzhong</author>
/// <contact>yingzhong@voiceaitech.com</contact>
/// <version>v1.0.0</version>
/// <describe>
/// It is a parallel server
///</describe>
/// <date>2020/2/23</date>
///***********************************************************************
#ifndef PARALLELSERVER_H
#define PARALLELSERVER_H

#include <sys/epoll.h>
#include <vector>
#include "Interface/IParallelServer.h"

class ParallelServer:public IParallelServer
{
public:
	// Construct the ParallelServer
	ParallelServer();
	
	// Detructe the ParallelServer
	virtual ~ParallelServer();
	
	// Get the Epollfd
	inline int GetEpollfd() const
	{
		return m_iEpollfd;
	}

	// Set the Epollfd
	inline void SetEpollfd(int iEpollfd)
	{
		m_iEpollfd = iEpollfd;
	}
private:
	// Forbid the copy ParallelServer
	ParallelServer(const ParallelServer& other) = delete;
	
	// Forbid the assigment of ParallelServer
	ParallelServer& operator=(const ParallelServer& other) = delete;
	
public:
	// Configure the server
	virtual bool Configure(int iServerPortNo, int iListenNum);

	// Start the server
	virtual bool Start();

	// Stop the server
	virtual bool Stop();

	// Get the error message 
	virtual std::string GetErrorMsg();

private:
	// Send the data to server
	bool Send(int iClientSocket,const char* pData, int iSendSize);

	// Receive data from the server
	bool Receive(int iClientSocket, char* pData, int iRevSize);

	// Prepare the environment
	bool PrepareEnvironment();

	// Set socket non-block
	void SetNonBlockStatus(int iSock);

private:
	// Get the disposed status
	inline bool GetDisposed() const
	{
		return m_bDisposed;
	}
	
	// Set the disposed status
	inline void SetDisposed(bool bDisposed)
	{
		m_bDisposed = bDisposed;
	}

	// Get the ErrorText
	inline std::string GetErrorText() const
	{
		return m_strErrorText;
	}

	// Set the ErrorText
	inline void SetErrorText(std::string strErrorText)
	{
		m_strErrorText = strErrorText;
	}

	// Get the ServerPortNo
	inline int GetServerPortNo() const
	{
		return m_iServerPortNo;
	}

	// Set the ServerPortNo
	inline void SetServerPortNo(int iServerPortNo)
	{
		m_iServerPortNo = iServerPortNo;
	}

	// Get the ListenSocket
	inline int GetListenSocket() const
	{
		return m_iListenSocket;
	}

	// Set the ListenSocket
	inline void SetListenSocket(int iListenSocket)
	{
		m_iListenSocket = iListenSocket;
	}

	// Get the ListenNum
	inline int GetListenNum() const
	{
		return m_iListenNum;
	}

	// Set the ListenNum
	inline void SetListenNum(int iListenNum)
	{
		m_iListenNum = iListenNum;
	}

private:
	// Epoll fd
	int m_iEpollfd;

	// Current epoll event
	epoll_event m_EpollEvent;

	// Epoll event array
	std::vector<epoll_event> m_EventTable;

	// Client socket
	int m_iClientSock;

	// Thread id
	pthread_t m_ThreadId;

	// Listen socket
	int m_iListenSocket;

	// Server port
	int m_iServerPortNo;

	// Listen number
	int m_iListenNum;

	// Error message 
	std::string m_strErrorText;

	// Disposed status
	bool m_bDisposed;
};
	
#endif //PARALLELSERVER_H
