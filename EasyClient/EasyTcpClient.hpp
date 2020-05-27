#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define _WINSOCK_DEPRECATED_NO_WARNINGS //for inet_pton()
	#define _CRT_SECURE_NO_WARNINGS
	#include <windows.h>
	#include <WinSock2.h>
	#pragma comment(lib, "ws2_32.lib")
#else
	#include <unistd.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/select.h>
	//在Unix下没有这些宏，为了兼容，自己定义
	#define SOCKET int
	#define INVALID_SOCKET  (SOCKET)(~0)
	#define SOCKET_ERROR            (-1)
#endif

//接收缓冲区的大小
#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240*5
#endif // !RECV_BUFF_SIZE


#include <iostream>
#include <string.h>
#include <stdio.h>
#include "MessageHeader.hpp"

using namespace std;

class EasyTcpClient
{
public:
	EasyTcpClient() :_sock(INVALID_SOCKET), _isConnect(false), _lastPos(0) {
		//memset(_recvBuff, 0, sizeof(_recvBuff));
		memset(_recvMsgBuff, 0, sizeof(_recvMsgBuff));
	}
	virtual ~EasyTcpClient() { CloseSocket(); }
public:
	void InitSocket();  //初始化socket
	void CloseSocket(); //关闭socket
	bool Onrun();       //处理网络消息
	bool isRun() { return ((_sock != INVALID_SOCKET) && _isConnect); }       //判断当前客户端是否在运行
	int  ConnectServer(const char* ip, unsigned int port); //连接服务器
	//使用RecvData接收任何类型的数据，然后将消息的头部字段传递给OnNetMessage()函数中，让其响应不同类型的消息
	int  RecvData();                                //接收数据
	virtual void OnNetMessage(DataHeader* header);  //响应网络消息
	int  SendData(DataHeader* header, int nLen)   ; //发送数据
private:
	SOCKET _sock;
	bool   _isConnect;                       //当前是否连接
	char   _recvMsgBuff[RECV_BUFF_SIZE];     //消息接收缓冲区
	int    _lastPos;                         //消息接收缓冲区数据的结尾位置
};

void EasyTcpClient::InitSocket()
{
	//如果之前有连接了，关闭旧连接，开启新连接
	if (isRun())
	{
		std::cout << "<Socket=" << (int)_sock << ">：关闭旧连接，建立了新连接" << std::endl;
		CloseSocket();
	}

#ifdef _WIN32
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		std::cout << "ERROR:建立socket失败!" << std::endl;
	}
	else {
		//std::cout << "<Socket=" << (int)_sock << ">：建立socket成功!" << std::endl;
	}
}

int EasyTcpClient::ConnectServer(const char* ip, unsigned int port)
{
	if (!isRun())
	{
		InitSocket();
	}

	//声明要连接的服务端地址（注意，不同平台的服务端IP地址也不同）
	struct sockaddr_in _sin = {};
#ifdef _WIN32
	_sin.sin_addr.S_un.S_addr = inet_addr(ip);
#else
	_sin.sin_addr.s_addr = inet_addr(ip);
#endif
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(port);

	//连接服务端
	int ret = connect(_sock, (struct sockaddr*)&_sin, sizeof(_sin));
	if (SOCKET_ERROR == ret) {
		std::cout << "<Socket=" << (int)_sock << ">：连接服务端(" << ip << "," << port << ")失败!" << std::endl;
	}
	else {
		_isConnect = true;
		//std::cout << "<Socket=" << (int)_sock << ">：连接服务端(" << ip << "," << port << ")成功!" << std::endl;
	}
	
	return ret;
}

void EasyTcpClient::CloseSocket()
{
	if (_sock != INVALID_SOCKET)
	{
#ifdef _WIN32
		closesocket(_sock);
		WSACleanup();
#else
		close(_sock);
#endif
		_sock = INVALID_SOCKET;
		_isConnect = false;
	}
}

bool EasyTcpClient::Onrun()
{
	if (isRun())
	{
		fd_set fdRead;
		FD_ZERO(&fdRead);
		FD_SET(_sock, &fdRead);

		struct timeval t = { 0,0 };
		int ret = select(_sock + 1, &fdRead, NULL, NULL, &t);
		if (ret < 0)
		{
			std::cout << "<Socket=" << _sock << ">：select出错！" << std::endl;
			return false;
		}
		if (FD_ISSET(_sock, &fdRead)) //如果服务端有数据发送过来，接收显示数据
		{
			FD_CLR(_sock, &fdRead);
			if (-1 == RecvData())
			{
				std::cout << "<Socket=" << _sock << ">：数据接收失败，或服务端已断开！" << std::endl;
				CloseSocket();
				return false;
			}
		}
		return true;
	}
	return false;
}

int EasyTcpClient::RecvData()
{
	char *_recvBuff = _recvMsgBuff + _lastPos;
	int _nLen = recv(_sock, _recvBuff, RECV_BUFF_SIZE- _lastPos, 0);
	if (_nLen < 0) {
		std::cout << "<Socket=" << _sock << ">：recv函数出错！" << std::endl;
		return -1;
	}
	else if (_nLen == 0) {
		std::cout << "<Socket=" << _sock << ">：接收数据失败，服务端已关闭!" << std::endl;
		return -1;
	}
	//std::cout << "_nLen=" << _nLen << std::endl;
	
	//（这一步不需要了)将获取的数据拷贝到消息缓冲区
	//memcpy(_recvMsgBuff + _lastPos, _recvBuff, _nLen);

	_lastPos += _nLen;
	//如果_recvMsgBuff中的数据长度大于等于DataHeader
	while (_lastPos >= sizeof(DataHeader))
	{
		DataHeader* header = (DataHeader*)_recvMsgBuff;
		//如果_lastPos的位置大于等于一个数据包的长度，那么就会这个数据包进行处理
		if (_lastPos >= header->dataLength)
		{
			//剩余未处理消息缓冲区的长度
			int nSize = _lastPos - header->dataLength;
			//处理网络消息
			OnNetMessage(header);
			//处理完成之后，将_recvMsgBuff中剩余未处理部分的数据前移
			memcpy(_recvMsgBuff, _recvMsgBuff + header->dataLength, nSize);
			_lastPos = nSize;
		}
		else {
			//消息缓冲区剩余数据不够一条完整消息
			break;
		}
	}
	return 0;
}

void EasyTcpClient::OnNetMessage(DataHeader* header)
{
	switch (header->cmd)
	{
	case CMD_LOGIN_RESULT:   //如果返回的是登录的结果
	{
		LoginResult* loginResult = (LoginResult*)header;
		//std::cout << "<Socket=" << _sock << ">，收到服务端数据：CMD_LOGIN_RESULT,数据长度：" << loginResult->dataLength << "，结果为：" << loginResult->result << std::endl;
	}
	break;
	case CMD_LOGOUT_RESULT:  //如果是退出的结果
	{
		LogoutResult* logoutResult = (LogoutResult*)header;
		//std::cout << "<Socket=" << _sock << ">，收到服务端数据：CMD_LOGOUT_RESULT,数据长度：" << logoutResult->dataLength << "，结果为：" << logoutResult->result << std::endl;
	}
	break;
	case CMD_NEW_USER_JOIN:  //有新用户加入
	{
		NewUserJoin* newUserJoin = (NewUserJoin*)header;
		//std::cout << "<Socket=" << _sock << ">，收到服务端数据：CMD_NEW_USER_JOIN,数据长度：" << newUserJoin->dataLength << "，新用户Socket为：" << newUserJoin->sock << std::endl;
	}
	break;
	case CMD_ERROR:  //错误消息
	{
		//错误消息的类型就是DataHeader的，因此直接使用header即可
		//std::cout << "<Socket=" << _sock << ">，收到服务端数据：CMD_ERROR，数据长度：" << header->dataLength << std::endl;
	}
	break;
	default:
	{
		//std::cout << "<Socket=" << _sock << ">，收到服务端数据：未知类型的消息，数据长度：" << header->dataLength << std::endl;
	}
	}
}

int EasyTcpClient::SendData(DataHeader* header,int nLen) 
{
	int ret = SOCKET_ERROR;
	if (isRun() && header)
	{
		ret = send(_sock, (const char*)header, nLen, 0);
		if (ret == SOCKET_ERROR) {
			CloseSocket();
			printf("Client:socket<%d>发送数据错误，关闭客户端连接\n", static_cast<int>(_sock));
		}
	}
	return ret;
}

#endif // !_EasyTcpClient_hpp_
