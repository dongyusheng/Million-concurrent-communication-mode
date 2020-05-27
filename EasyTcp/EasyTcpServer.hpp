#ifndef _EasyTcpClient_hpp_
#define _EasyTcpClient_hpp_

#ifdef _WIN32
#define FD_SETSIZE 10240
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


#ifndef RECV_BUFF_SIZE
#define RECV_BUFF_SIZE 10240*5        //接收缓冲区的大小
#endif // !RECV_BUFF_SIZE

#ifndef SEND_BUFF_SIZE
#define SEND_BUFF_SIZE RECV_BUFF_SIZE //发送缓冲区的大小
#endif // !SEND_BUFF_SIZE

#include <iostream>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional> 
#include <map>
#include <memory>
#include "MessageHeader.hpp"
#include "CELLTimestamp.hpp"
#include "CELLTask.hpp"
//using namespace std;

class CellServer;

//客户端数据类型，用来封装一个客户端
class ClientSocket
{
public:
	ClientSocket(SOCKET sockfd = INVALID_SOCKET) :_sock(sockfd), _lastRecvPos(0), _lastSendPos(0) {
		memset(_recvMsgBuff, 0, sizeof(_recvMsgBuff));
		memset(_sendMsgBuff, 0, sizeof(_sendMsgBuff));
	}

	SOCKET sockfd() { return _sock; }
	char   *recvMsgBuff() { return _recvMsgBuff; }
	int    getRecvLastPos() { return _lastRecvPos; }
	void   setRecvLastPos(int pos) { _lastRecvPos = pos; }

	char   *sendMsgBuff() { return _sendMsgBuff; }
	int    getSendLastPos() { return _lastSendPos; }
	void   setSendLastPos(int pos) { _lastSendPos = pos; }

	int    SendData(std::shared_ptr<DataHeader>& header);
private:
	SOCKET _sock;                         //客户端socket

	char   _recvMsgBuff[RECV_BUFF_SIZE];  //消息接收缓冲区
	int    _lastRecvPos;                  //消息接收缓冲区的数据尾部位置

	char   _sendMsgBuff[SEND_BUFF_SIZE];  //消息发送缓冲区
	int    _lastSendPos;                  //消息发送缓冲区的数据尾部位置
};

//网络事件接口
class INetEvent {
public:
	virtual void OnClientJoin(std::shared_ptr<ClientSocket>& pClient) = 0;     //客户端加入事件
	virtual void OnClientLeave(std::shared_ptr<ClientSocket>& pClient) = 0;    //客户端离开事件
	virtual void OnNetMsg(CellServer* pCellServer, std::shared_ptr<ClientSocket>& pClient, DataHeader* header) = 0; //接收到客户端消息事件
	virtual void OnNetRecv(std::shared_ptr<ClientSocket>& pClient) = 0;        //recv函数执行事件
};

//发送消息的任务
class CellSendMsg2ClientTask :public CellTask
{
public:
	CellSendMsg2ClientTask(std::shared_ptr<ClientSocket>& pClient, std::shared_ptr<DataHeader>& header)
		:_pClient(pClient), _pHeader(header) {}

	void doTask()
	{
		_pClient->SendData(_pHeader);
	}
private:
	std::shared_ptr<ClientSocket> _pClient; //发送给哪个客户端
	std::shared_ptr<DataHeader>   _pHeader; //要发送的数据的头指针
};

//处理客户的从服务器类
class CellServer
{
public:
	CellServer(SOCKET sock = INVALID_SOCKET) :_sock(sock), maxSock(_sock), _pthread(nullptr), _pNetEvent(nullptr), _clients_change(false) {
		memset(&_fdRead_bak, 0, sizeof(_fdRead_bak));
	}
	~CellServer() { CloseSocket(); }
public:
	bool   isRun() { return _sock != INVALID_SOCKET; }
	void   CloseSocket();
public:
	size_t getClientCount() { return _clients.size() + _clientsBuff.size(); } //返回当前客户端的数量
	void   setEventObj(INetEvent* event) { _pNetEvent = event; }              //设置事件对象，此处绑定的是EasyTcpServer
public:
	bool   Onrun();
	void   AddClient(std::shared_ptr<ClientSocket>& pClient)  //讲客户端加入到客户端连接缓冲队列中
	{
		//自解锁
		std::lock_guard<std::mutex> lock(_mutex);
		//_mutex.lock(); 当然也可以使用互斥锁
		_clientsBuff.push_back(pClient);
		//_mutex.unlock();
	}
	int    RecvData(std::shared_ptr<ClientSocket>& pClient);     //接收数据
	void   OnNetMessage(std::shared_ptr<ClientSocket>& pClient, DataHeader* header);//处理网络消息
	void   Start() {
		//启动当前服务线程
		//创建一个线程，线程执行函数为Onrun()，其实可以不传递this，但是为了更安全，可以传递this给Onrun()
		_pthread = new std::thread(std::mem_fn(&CellServer::Onrun), this);

		//启动任务管理
		_taskServer.Start();
	}
public:
	void addSendTask(std::shared_ptr<ClientSocket>& pClient, std::shared_ptr<DataHeader>& header)
	{
		auto task = std::make_shared<CellSendMsg2ClientTask>(pClient, header);
		_taskServer.addTask((CellTaskPtr&)task);
	}
private:
	SOCKET                     _sock;         //服务端的套接字
	std::map<SOCKET, std::shared_ptr<ClientSocket>> _clients; //真正存储客户端
	std::vector<std::shared_ptr<ClientSocket>> _clientsBuff;  //存储客户端连接缓冲队列，之后会被加入到_clients中去
	SOCKET                     maxSock;       //当前最大的文件描述符值，select的参数1要使用
											  //char _recvBuff[RECV_BUFF_SIZE];           //接收缓冲区
	std::mutex                 _mutex;        //互斥锁
	std::thread*               _pthread;      //当前子服务端执行的线程
	INetEvent*                 _pNetEvent;
	fd_set                     _fdRead_bak;   //用来保存当前的fd_set
	bool                       _clients_change;//当前是否有新客户端加入进来
private:
	CellTaskServer _taskServer;
};

//服务器主类
class EasyTcpServer :public INetEvent
{
public:
	EasyTcpServer() :_sock(INVALID_SOCKET), _msgCount(0), _recvCount(0), _clientCount(0) {}
	virtual ~EasyTcpServer() { CloseSocket(); }
public:
	void InitSocket();  //初始化socket
	int  Bind(const char* ip, unsigned short port);    //绑定端口号
	int  Listen(int n); //监听端口号
	SOCKET Accept();    //接收客户端连接
	void addClientToCellServer(std::shared_ptr<ClientSocket>& pClient);//将新客户加入到CellServer的客户端连接缓冲队列中
	void Start(int nCellServer);                      //创建从服务器，并运行所有的从服务器。(参数为从服务器的数量)
	void CloseSocket(); //关闭socket
	bool isRun() { return _sock != INVALID_SOCKET; }  //判断当前服务端是否在运行
	bool Onrun();       //处理网络消息

	void time4msg();    //每1秒统计一次收到的数据包的数量
public:
	//客户端加入事件(这个是线程安全的，因为其只会被主服务器(自己)调用)
	virtual void OnClientJoin(std::shared_ptr<ClientSocket>& pClient)override { _clientCount++; }
	
	//客户端离开事件，这个里面做的事情比较简单，只是将当前客户端的数量--（如果从服务器不止一个，那么此函数不是线程安全的，因为这个函数会被多个从服务器调用的）
	virtual void OnClientLeave(std::shared_ptr<ClientSocket>& pClient)override { _clientCount--; }
	
	//接收到客户端消息事件，将数据包的数量增加（如果从服务器不止一个，那么此函数不是线程安全的，因为这个函数会被多个从服务器调用的）
	//参数1，代表哪一个CellServer来处理这个消息的
	virtual void OnNetMsg(CellServer* pCellServer, std::shared_ptr<ClientSocket>& pClient, DataHeader* header)override { _msgCount++; }
	
	//recv事件
	virtual void OnNetRecv(std::shared_ptr<ClientSocket>& pClient)override { _recvCount++; }
private:
	SOCKET                     _sock;        //服务端套接字
	std::vector<std::shared_ptr<CellServer>>   _cellServers; //存放从服务端对象
	CELLTimestamp              _tTime;       //计时器
	std::atomic_int            _clientCount; //客户端的数量(这里采用一个原子操作，没什么特殊原因，使用玩玩罢了,下同)
	std::atomic_int            _msgCount;    //表示服务端接收到客户端数据包的数量
	std::atomic_int            _recvCount;   //recv()函数执行的次数
};

int ClientSocket::SendData(std::shared_ptr<DataHeader>& header) {
	int ret = SOCKET_ERROR;

	//要发送的数据的长度
	int nSendLen = header->dataLength;
	const char* pSendData = (const char*)header.get();

	//在下面第一个if之后，如果nSendLen仍然大于SEND_BUFF_SIZE，那么就需要继续执行if，继续发送数据
	while (true)
	{
		//如果当前"要发送的数据的长度+缓冲区数据结尾位置"之和总的缓冲区大小，说明缓冲区满了，那么将整个缓冲区都发送出去
		if (_lastSendPos + nSendLen >= SEND_BUFF_SIZE)
		{
			//计算缓冲区还剩多少空间
			int nCopyLen = SEND_BUFF_SIZE - _lastSendPos;
			//然后将nCopyLen长度的pSendData数据拷贝到缓冲区中
			memcpy(_sendMsgBuff + _lastSendPos, pSendData, nCopyLen);
			//计算剩余数据位置
			pSendData += nCopyLen;
			//计算剩余数据长度
			nSendLen -= nCopyLen;
			ret = send(_sock, _sendMsgBuff, SEND_BUFF_SIZE, 0);
			//缓冲区数据尾部置为0
			_lastSendPos = 0;

			if (ret = SOCKET_ERROR)
				return -1;
		}
		//如果发送缓冲区还没满，那么将这条消息放到缓冲区中，而不直接发送
		else
		{
			memcpy(_sendMsgBuff + _lastSendPos, pSendData, nSendLen);
			//更新缓冲区数据尾部
			_lastSendPos += nSendLen;
			//直接退出，不进行while
			break;
		}
	}

	return ret;
}

void EasyTcpServer::InitSocket()
{
#ifdef _WIN32
	WORD ver = MAKEWORD(2, 2);
	WSADATA dat;
	WSAStartup(ver, &dat);
#endif

	//建立socket
	_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == _sock) {
		std::cout << "Server：创建socket成功" << std::endl;
	}
	else {
		std::cout << "Server：创建socket成功" << std::endl;
	}
}

int EasyTcpServer::Bind(const char* ip, unsigned short port)
{
	if (!isRun())
		InitSocket();

	//初始化服务端地址
	struct sockaddr_in _sin = {};
#ifdef _WIN32
	if (ip)
		_sin.sin_addr.S_un.S_addr = inet_addr(ip);
	else
		_sin.sin_addr.S_un.S_addr = INADDR_ANY;
#else
	if (ip)
		_sin.sin_addr.s_addr = inet_addr(ip);
	else
		_sin.sin_addr.s_addr = INADDR_ANY;
#endif
	_sin.sin_family = AF_INET;
	_sin.sin_port = htons(port);

	//绑定服务端地址
	int ret = ::bind(_sock, (struct sockaddr*)&_sin, sizeof(_sin));
	if (SOCKET_ERROR == ret) {
		if (ip)
			std::cout << "Server：绑定地址(" << ip << "," << port << ")失败!" << std::endl;
		else
			std::cout << "Server：绑定地址(INADDR_ANY," << port << ")失败!" << std::endl;
	}
	else {
		if (ip)
			std::cout << "Server：绑定地址(" << ip << "," << port << ")成功!" << std::endl;
		else
			std::cout << "Server：绑定地址(INADDR_ANY," << port << ")成功!" << std::endl;
	}
	return ret;
}

void EasyTcpServer::CloseSocket()
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
	}
}

int EasyTcpServer::Listen(int n)
{
	//监听网络端口
	int ret = listen(_sock, n);
	if (SOCKET_ERROR == ret)
		std::cout << "Server：监听网络端口失败!" << std::endl;
	else
		std::cout << "Server：监听网络端口成功!" << std::endl;
	return ret;
}

SOCKET EasyTcpServer::Accept()
{
	//用来保存客户端地址
	struct sockaddr_in _clientAddr = {};
	int nAddrLen = sizeof(_clientAddr);
	SOCKET _cSock = INVALID_SOCKET;

	//接收客户端连接
#ifdef _WIN32
	_cSock = accept(_sock, (struct sockaddr*)&_clientAddr, &nAddrLen);
#else
	_cSock = accept(_sock, (struct sockaddr*)&_clientAddr, (socklen_t*)&nAddrLen);
#endif
	if (INVALID_SOCKET == _cSock) {
		std::cout << "Server：接收到无效客户端!" << std::endl;
	}
	else {
		//通知其他已存在的所有客户端，有新的客户端加入
		//NewUserJoin newUserInfo(static_cast<int>(_cSock));
		//SendDataToAll(&newUserInfo);

		//将新连接的客户端加入到从服务器的客户端缓冲队列中
		std::shared_ptr<ClientSocket> newClient = std::make_shared<ClientSocket>(_cSock);
		addClientToCellServer(newClient);
		OnClientJoin(newClient); //相应客户端加入事件，其函数会将客户端的数量++

		//std::cout << "Server：接受到新的客户端(" << _clients.size() << ")连接,IP=" << inet_ntoa(_clientAddr.sin_addr)
		//	<< ",Socket=" << static_cast<int>(_cSock) << std::endl;
	}
	return _cSock;
}

void EasyTcpServer::addClientToCellServer(std::shared_ptr<ClientSocket>& pClient)
{
	//在_cellServers中寻找，哪一个CellServer其处理的客户数量最少，那么就将新客户加入到这个CellServer对象中去
	auto pMinServer = _cellServers[0];
	for (auto pCellServer : _cellServers)
	{
		if (pMinServer->getClientCount() > pCellServer->getClientCount())
		{
			pMinServer = pCellServer;
		}
	}
	pMinServer->AddClient(pClient);
}

void EasyTcpServer::Start(int nCellServer)
{
	for (int i = 0; i < nCellServer; ++i)
	{
		auto ser = std::make_shared<CellServer>(_sock);
		_cellServers.push_back(ser);
		ser->setEventObj(this);
		ser->Start();
	}
}

bool EasyTcpServer::Onrun()
{
	if (isRun())
	{
		time4msg(); //统计当前接收到的数据包的数量

		fd_set fdRead;
		//fd_set fdWrite;
		//fd_set fdExp;
		FD_ZERO(&fdRead);
		//FD_ZERO(&fdWrite);
		//FD_ZERO(&fdExp);
		FD_SET(_sock, &fdRead);
		//FD_SET(_sock, &fdWrite);
		//FD_SET(_sock, &fdExp);

		struct timeval t = { 0,0 };
		int ret = select(_sock + 1, &fdRead, nullptr, nullptr, &t);
		if (ret < 0)
		{
			std::cout << "Server：select出错！" << std::endl;
			//select出错，那么就不能再继续运行select，出错之后，调用CloseSocket()，
			//关闭所有服务端、及客所有户端套接字，那么isRun()就会返回false，从而终止server.cpp程序运行
			CloseSocket();
			return false;
		}
		if (FD_ISSET(_sock, &fdRead))//如果一个客户端连接进来，那么服务端的socket就会变为可读的，此时我们使用accept来接收这个客户端
		{
			FD_CLR(_sock, &fdRead);
			Accept();
			return true;
		}
		return true;
	}
	return false;
}

void EasyTcpServer::time4msg()
{
	auto t = _tTime.getElapsedSecond();
	if (t >= 1.0)
	{
		//msgCount,_recvCount为什么要除以t：
		//因为我们要每1秒钟打印一次接收到的数据包，如果这个函数调用的时候时间差大于1秒，那么可以将recvCount/t，获得比较平均的数据包数量/recv执行次数
		printf("time<%lf>,thread numer<%d>,client number<%d>,msgCount<%d>,recvCount<%d>\n",
			t, _cellServers.size(), static_cast<int>(_clientCount), static_cast<int>(_msgCount / t), static_cast<int>(_recvCount / t));
		_recvCount = 0;
		_msgCount = 0;
		_tTime.update();
	}
}

void CellServer::CloseSocket()
{
	if (_sock != INVALID_SOCKET)
	{
#ifdef _WIN32
		//将所有的客户端套接字关闭
		for (auto iter : _clients)
		{
			closesocket(iter.second->sockfd());
		}
		//关闭服务端套接字
		closesocket(_sock);

		//因为这是从服务器，所以就不要清理套接字环境了，放置主服务器的套接字环境被清除
		//WSACleanup();
#else
		for (auto iter : _clients)
		{
			close(iter.second->sockfd());
		}
		close(_sock);
#endif
		_clients.clear();
		_sock = INVALID_SOCKET;
		delete _pthread;
	}
}

bool CellServer::Onrun()
{
	while (isRun())
	{
		//如果客户端缓冲队列_clientsBuff中有新客户，那么就将其加入到_clients中
		if (_clientsBuff.size() > 0)
		{
			//自解锁lock_guard，作用域结束后自动释放锁，因此if执行结束之后，_mutex就被释放了
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pClient : _clientsBuff)
			{
				_clients[pClient->sockfd()] = pClient;
			}
			_clientsBuff.clear();
			_clients_change = true;
		}

		//如果没有客户，那么休眠一秒然后继续下一次循环
		if (_clients.empty())
		{
			std::chrono::milliseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		fd_set fdRead;
		FD_ZERO(&fdRead);
		//在主线程的select中已经对主服务端的_sock进行查询了，所以从服务器就不需要再将_sock加入到fd_set中了，否则两个地方同时操作会出错
		//FD_SET(_sock, &fdRead);

		//根据_fdRead_change判断是否有新客户端加入，如果有那么就进行新的FD_SET
		if (_clients_change)
		{
			_clients_change = false;
			for (auto iter : _clients)
			{
				FD_SET(iter.second->sockfd(), &fdRead);
				if (maxSock < iter.second->sockfd())
					maxSock = iter.second->sockfd();
			}
			//将更新后的fd_set保存到_fdRead_bak中
			memcpy(&_fdRead_bak, &fdRead, sizeof(_fdRead_bak));
		}
		//否则直接拷贝，不用再循环FD_SET了
		else
			memcpy(&fdRead, &_fdRead_bak, sizeof(_fdRead_bak));


		//从服务器一般只用来收取数据，所以这里设置为阻塞的也可以
		//struct timeval t = { 1,0 };
		int ret = select(maxSock + 1, &fdRead, nullptr, nullptr, nullptr);
		if (ret < 0)
		{
			std::cout << "Server：select出错！" << std::endl;
			CloseSocket();
			return false;
		}

#ifdef _WIN32
		//如果是WIN下运行，fd_set拥有fd_count与fd_array成员
		//我们可以遍历fd_set，然后从中获取数据，不需要使用FD_ISSET了
		for (int n = 0; n < fdRead.fd_count; n++)
		{
			auto iter = _clients.find(fdRead.fd_array[n]);
			//如果RecvData出错，那么就将该客户端从_client中移除
			if (-1 == RecvData(iter->second))
			{
				if (_pNetEvent)
					_pNetEvent->OnClientLeave(iter->second); //通知主服务器有客户端退出
				//delete iter->second;
				_clients.erase(iter->first);
				_clients_change = true; //这个要设置为true，因为有客户端退出了，需要重新进行FD_SET
			}
		}
#else
		//如果在UNIX下，fd_set无fd_count与fd_array成员，我们只能遍历_clients数组
		//遍历_clients map容器中所有的客户端，然后从中获取数据
		for (auto iter : _clients)
		{
			//因为_clients是一个map，因此每次iter返回一个pair,其first成员为key(SOCKET)，value成员为value(ClientSocket)
			if (FD_ISSET(iter.second->sockfd(), &fdRead))
			{
				//如果RecvData出错，那么就将该客户端从_client中移除
				if (-1 == RecvData(iter.second))
				{
					if (_pNetEvent)
						_pNetEvent->OnClientLeave(iter.second); //通知主服务器有客户端退出
					_clients.erase(iter.first);
					_clients_change = true; //原因同上
				}
			}
		}
#endif // _WIN32

	}
	return false;
}

int CellServer::RecvData(std::shared_ptr<ClientSocket>& pClient)
{
	char *_recvBuff = pClient->recvMsgBuff() + pClient->getRecvLastPos();
	//先将数据接收到_recvBuff缓冲区中
	int _nLen = recv(pClient->sockfd(), _recvBuff, RECV_BUFF_SIZE - pClient->getRecvLastPos(), 0);
	_pNetEvent->OnNetRecv(pClient);
	if (_nLen < 0) {
		//std::cout << "recv函数出错！" << std::endl;
		return -1;
	}
	else if (_nLen == 0) {
		//std::cout << "客户端<Socket=" << pClient->sockfd() << ">：已退出！" << std::endl;
		return -1;
	}

	//（不需要这一步了）将数据从_recvBuff中拷贝到客户端的缓冲区中
	//memcpy(pClient->msgBuff() + pClient->getLastPos(), _recvBuff, _nLen);

	pClient->setRecvLastPos(pClient->getRecvLastPos() + _nLen);
	//判断客户端的缓冲区中数据结尾的位置，如果有一个DataHeader的大小那么就可以对数据进行处理
	while (pClient->getRecvLastPos() >= sizeof(DataHeader))
	{
		//获取缓冲区的首指针
		DataHeader* header = (DataHeader*)pClient->recvMsgBuff();
		//如果当前缓冲区中数据结尾的位置大于等于一个数据包的大小，那么就对数据进行处理
		if (pClient->getRecvLastPos() >= header->dataLength)
		{
			//先保存剩余未处理消息缓冲区的长度
			int nSize = pClient->getRecvLastPos() - header->dataLength;
			//处理网络消息
			OnNetMessage(pClient, header);
			//处理完成之后，将_recvMsgBuff中剩余未处理部分的数据前移
			memcpy(pClient->recvMsgBuff(), pClient->recvMsgBuff() + header->dataLength, nSize);
			pClient->setRecvLastPos(nSize);
		}
		else {
			//消息缓冲区剩余数据不够一条完整消息
			break;
		}
	}
	return 0;
}

void CellServer::OnNetMessage(std::shared_ptr<ClientSocket>& pClient, DataHeader* header)
{
	//调用主服务OnNetMsg事件
	_pNetEvent->OnNetMsg(this, pClient, header);

	switch (header->cmd)
	{
	case CMD_LOGIN: //如果是登录
	{
		//Login *login = (Login*)header;
		//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的消息CMD_LOGIN，用户名：" << login->userName << "，密码：" << login->PassWord << std::endl;

		//此处可以判断用户账户和密码是否正确等等（省略）

		//返回登录的结果给客户端
		auto ret = std::make_shared<LoginResult>();
		//在_taskServer()内部封装一个发送消息任务，然后执行该发送任务
		this->addSendTask(pClient, (std::shared_ptr<DataHeader>)ret);
	}
	break;
	case CMD_LOGOUT:  //如果是退出
	{
		//Logout *logout = (Logout*)header;
		//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的消息CMD_LOGOUT，用户名：" << logout->userName << std::endl;

		//返回退出的结果给客户端
		auto ret = std::make_shared<LogoutResult>();
		this->addSendTask(pClient, (std::shared_ptr<DataHeader>)ret);
	}
	break;
	default:  //如果有错误
	{
		//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的未知消息消息" << std::endl;

		//返回错误给客户端，DataHeader默认为错误消息
		auto ret = std::make_shared<DataHeader>();
		this->addSendTask(pClient, ret);
	}
	break;
	}
}

#endif