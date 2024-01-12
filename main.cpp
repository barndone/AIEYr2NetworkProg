#include <WinSock2.h> 
#include <WS2tcpip.h> 
#include <iostream>
#include <cassert>

#include <chrono>
#include <thread>
#include <string>
#include <thread>
#include <vector>
#include <mutex>



bool shouldUpdate = false;
//	remote addr
addrinfo* remoteAddr = nullptr;

void PrintIncomingData(SOCKET curSocket, timeval curSocketTimeout, unsigned char buffer[], const size_t BUFFER_SIZE)
{

	static std::mutex receiveMutex;

	receiveMutex.lock();

	//	fd_set curSocketDesc;
	//	FD_ZERO(&curSocketDesc);
	//	FD_SET(curSocket, &curSocketDesc);
	//	int status = select((int)curSocket, &curSocketDesc, nullptr, nullptr, &curSocketTimeout);
	//	
	//	//	status might be---
	//	//	0 => socket timeout
	//	//	SOCKET_ERROR => an error occured (duh)
	//	//	status > 0 => WE HAVE DATA GAMERS!
	//	
	//	if (status > 0)
	//	{
		//	incoming address info

	while (true)
	{
		if (shouldUpdate)
		{

			addrinfo incomingAddr;
			memset(&incomingAddr, 0, sizeof(incomingAddr));
			socklen_t incomingAddrLen = sizeof(incomingAddr);

			remoteAddr = &incomingAddr;

			int bytesReceived = recvfrom(
				curSocket,						//	local socket
				(char*)buffer,					//	ptr to buffer (incoming data goes here)	
				BUFFER_SIZE,					//	size of buffer
				0,								//	special flags / settings
				(sockaddr*)&incomingAddr,		//	ptr to remote addr (incoming address goes here!)
				&incomingAddrLen);				//	length of remote address

			//	the value returned by recvfrom corresponds to the number of byte that we received in that message, if it's greater than zero (not false) we have data to work with
			if (bytesReceived)
			{
				//	unsafe: assume message is a c-string and is shorter in length
				//			and will fit within the buffer we've allocated

				buffer[bytesReceived] = '\0'; //	insert NULL-term char

				//	print it to stdout
				auto time_point = std::chrono::system_clock::now();
				std::cout << "RECEIVED [" << std::chrono::system_clock::to_time_t(time_point) << "] " << (char*)buffer << std::endl;

				shouldUpdate = false;
			}
		}
	}

	//	}

	receiveMutex.unlock();
}

void SendOutgoingData(SOCKET curSocket, const size_t BUFFER_SIZE)
{

	static std::mutex outgoingMutex;
	
	outgoingMutex.lock();

	while (true)
	{
		std::string input;
		std::getline(std::cin, input);
		//const char* msg = "Beep Boop Im a Shoe";
		const size_t msgLen = strlen(input.c_str());
		assert(msgLen + 1 < BUFFER_SIZE);

		//	send the message to the server
		int success = sendto(
			curSocket,						//	socket to send on
			input.c_str(),					//	message/data to send
			(int)msgLen + 1,				//	length of the data in bytes
			0,								//	special flags if any
			remoteAddr->ai_addr,			//	dest address data
			remoteAddr->ai_addrlen);		//	length of dest.addr in bytes

		if (success)
		{
			auto time_point = std::chrono::system_clock::now();
			std::cout << "SENT [" << std::chrono::system_clock::to_time_t(time_point) << "] " << (char*)input.c_str() << std::endl;
		}
	}


	outgoingMutex.unlock();
}

int main(int argc, char** argv)
{
	//	setp 1 init server
	if (argc < 2) { return -1; }

	bool isServer = strcmp(argv[1], "server") == 0;
	bool isClient = !isServer;

	if (isServer)
	{
		std::cout << "Server" << std::endl;
	}

	if (isClient)
	{
		std::cout << "Client" << std::endl;
	}

	//	step 1 init winsock
	WSADATA wsaData;

	//	will return an error code
	int success = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (success != 0)
	{
		std::cerr << "ERROR: failed to initialize WinSock :'(" << std::endl;
		return -1;
	}

	//
	//	resolve local and server addresses
	//
	addrinfo config;
	memset(&config, 0, sizeof(addrinfo));		// zero out memory for that object (c-paradigm)
	config.ai_family = AF_INET;					//	type of address
	config.ai_socktype = SOCK_DGRAM;			//	type of socket
	config.ai_protocol = IPPROTO_UDP;			//	type of connection TCP/UDP
	config.ai_flags = AI_PASSIVE;				//	other hints or settings

	//	local address
	addrinfo* localAddr = nullptr;

	if (isServer)
	{
		success = getaddrinfo(
			nullptr,		//	address
			"7777",			//	the port - will find the best one! (@ AIE use 7777)
			&config,		//	config that will be used
			&localAddr);	//	pointer to the address we are passing to
	}

	if (isClient)
	{
		success = getaddrinfo(
			nullptr,		//	address
			"0",			//	the port - will find the best one! (@ AIE use 7777)
			&config,		//	config that will be used
			&localAddr);	//	pointer to the address we are passing to
	}

	if (success != 0)
	{
		std::cerr << "ERROR: local getaddrinfo failed! :'(" << std::endl;
		WSACleanup();
		return 1;
	}




	if (argc < 3)
	{
		success = getaddrinfo(
			"127.0.0.1",
			"7777",
			&config,
			&remoteAddr);
	}
	else
	{
		success = getaddrinfo(
			argv[2],
			"7777",
			&config,
			&remoteAddr);
	}
	


	if (success != 0)
	{
		std::cerr << "ERROR: remote getaddrinfo failed! :'(" << std::endl;
		WSACleanup();
		return 1;
	}

	//	now that we have a local address, we can create a socket and bind it to the local address so we can send data from it!
	SOCKET curSocket = INVALID_SOCKET;
	curSocket = socket(localAddr->ai_family, localAddr->ai_socktype, localAddr->ai_protocol);

	//	check if its an invalid socket
	if (curSocket == INVALID_SOCKET)
	{
		std::cerr << "ERROR: socket creation failed! :'(" << std::endl;
		freeaddrinfo(localAddr);
		WSACleanup();
		return 1;
	}

	// if success- time to bind!
	success = bind(
		curSocket,								//	socket to bind to
		localAddr->ai_addr,						//	address itself
		(int)localAddr->ai_addrlen);			//	length of the address

	if (success == SOCKET_ERROR)
	{
		std::cerr << "ERROR: socket binding failed! :'(" << std::endl;
		freeaddrinfo(localAddr);
		closesocket(curSocket);
		WSACleanup();
		return 1;
	}

	//	if the binding is successful, we can free up the local socket
	freeaddrinfo(localAddr);

	//
	//	later, to unbind the socket we can call closesocket(curSocket)
	//

	//	poll for network traffic
	timeval curSocketTimeout;
	curSocketTimeout.tv_sec = 2;	//	2 seconds
	curSocketTimeout.tv_usec = 0;	//	0 microseconds

	//	specify BUFFER_SIZE
	const size_t BUFFER_SIZE = 1400;
	unsigned char buffer[BUFFER_SIZE];

	//	after declairing our timeout and our buffer, we can enter our application loop!

	//std::thread incomingText(PrintIncomingData);

	std::vector<std::thread> threads;

	
	std::thread incoming(PrintIncomingData, curSocket, curSocketTimeout, buffer, BUFFER_SIZE);
	std::thread outgoing(SendOutgoingData, curSocket, BUFFER_SIZE);

	if (isClient)
	{
		
		std::string input = "";
		//const char* msg = "Beep Boop Im a Shoe";
		const size_t msgLen = strlen(input.c_str());
		assert(msgLen + 1 < BUFFER_SIZE);

		//	send the message to the server
		int success = sendto(
			curSocket,						//	socket to send on
			input.c_str(),					//	message/data to send
			(int)msgLen + 1,				//	length of the data in bytes
			0,								//	special flags if any
			remoteAddr->ai_addr,			//	dest address data
			remoteAddr->ai_addrlen);		//	length of dest.addr in bytes
	}

	while (true)
	{
		fd_set curSocketDesc;
		FD_ZERO(&curSocketDesc);
		FD_SET(curSocket, &curSocketDesc);
		int status = select((int)curSocket, &curSocketDesc, nullptr, nullptr, &curSocketTimeout);

		if (status > 0)
		{
			shouldUpdate = true;
		}
	}

	return 0;
}
	