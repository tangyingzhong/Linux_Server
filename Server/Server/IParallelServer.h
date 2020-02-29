///************************************************************************
/// <copyrigth>Voice AI Technology Of ShenZhen</copyrigth>
/// <author>tangyingzhong</author>
/// <contact>yingzhong@voiceaitech.com</contact>
/// <version>v1.0.0</version>
/// <describe>
/// Offer interfaces for parallel server
///</describe>
/// <date>2020/2/23</date>
///***********************************************************************
#ifndef IPARALLELSERVER_H
#define IPARALLELSERVER_H

#include <iostream>

class IParallelServer
{
public:
	// Detructe the IParallelServer
	virtual ~IParallelServer() {	}

public:
	// Configure the server
	virtual bool Configure(int iServerPortNo, int iListenNum) = 0;

	// Start the server
	virtual bool Start() = 0;

	// Stop the server
	virtual bool Stop() = 0;

	// Get the error message 
	virtual std::string GetErrorMsg() = 0;
};

#endif //IPARALLELSERVER_H
