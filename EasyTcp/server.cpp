#include "EasyTcpServer.hpp"
#include "MessageHeader.hpp"
#include "Alloctor.h"
#include "CELLObjectPoll.hpp"

/*class MyServer :public EasyTcpServer
{
public:
	//客户端加入事件
	virtual void OnClientJoin(ClientSocket* pClient)override {
		_clientCount++;
		printf("client<%d> join\n", pClient->sockfd());
	}
	//客户端离开事件
	virtual void OnClientLeave(ClientSocket* pClient)override {
		_clientCount--;
		printf("client<%d> leave\n", pClient->sockfd());
	}
	//接收到客户端消息事件
	virtual void OnNetMsg(ClientSocket* pClient, DataHeader* header)override 
	{
		_recvCount++;
		switch (header->cmd)
		{
		case CMD_LOGIN: //如果是登录
		{
			//Login *login = (Login*)header;
			//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的消息CMD_LOGIN，用户名：" << login->userName << "，密码：" << login->PassWord << std::endl;

			//此处可以判断用户账户和密码是否正确等等（省略）

			//返回登录的结果给客户端
			LoginResult ret;
			pClient->SendData(&ret);
		}
		break;
		case CMD_LOGOUT:  //如果是退出
		{
			//Logout *logout = (Logout*)header;
			//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的消息CMD_LOGOUT，用户名：" << logout->userName << std::endl;

			//返回退出的结果给客户端
			LogoutResult ret;
			pClient->SendData(&ret);
		}
		break;
		default:  //如果有错误
		{
			//std::cout << "服务端：收到客户端<Socket=" << pClient->sockfd() << ">的未知消息消息" << std::endl;

			//返回错误给客户端，DataHeader默认为错误消息
			DataHeader ret;
			pClient->SendData(&ret);
		}
		break;
		}
	}
};*/


class ClassA :public ObjectPollBase<ClassA>
{
public:

};


int main()
{
	/*EasyTcpServer server;
	server.Bind("192.168.0.105", 4567);
	server.Listen(5);
	server.Start(4);

	while (server.isRun())
	{
		server.Onrun();
	}

	server.CloseSocket();
	std::cout << "服务端停止工作!" << std::endl;

	getchar();  //防止程序一闪而过*/
	return 0;
}