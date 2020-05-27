#include "EasyTcpClient.hpp"
#include "CELLTimestamp.hpp"
#include <thread>
#include <atomic>

bool g_bRun = false;
const int cCount = 1000;      //客户端的数量
const int tCount = 4;          //线程的数量
std::atomic_int sendCount = 0; //send()函数执行的次数
std::atomic_int readyCount = 0;//代表已经准备就绪的线程数量
EasyTcpClient* client[cCount]; //客户端的数组

void cmdThread();
void sendThread(int id);

int main()
{
	g_bRun = true;

	//UI线程，可以输入命令
	std::thread t(cmdThread);
	t.detach();

	//启动发送线程
	for (int n = 0; n < tCount; ++n)
	{
		std::thread t(sendThread, n + 1);
		t.detach();
	}

	//每1秒中打印一次信息(其中包括send()函数的执行次数)
	CELLTimestamp tTime;
	while (true)
	{
		auto t = tTime.getElapsedSecond();
		if (t >= 1.0)
		{
			printf("time<%lf>,thread numer<%d>,client number<%d>,sendCount<%d>\n",
				t, tCount, cCount, static_cast<int>(sendCount / t));
			sendCount = 0;
			tTime.update();
		}
		Sleep(1);
	}

	return 0;
}

void cmdThread()
{
	char cmdBuf[256] = {};
	while (true)
	{
		std::cin >> cmdBuf;
		if (0 == strcmp(cmdBuf, "exit"))
		{
			g_bRun = false;
			break;
		}
		else {
			std::cout << "命令不识别，请重新输入" << std::endl;
		}
	}
}

void sendThread(int id)
{
	/*
		下面这几个变量是为了平均每个线程创建的客户端的数量：
			例如，本次测试时客户端数量为1000，线程数量为4，那么每个线程应该创建250个客户端
				线程1：c=250，begin=0,end=250
				线程2：c=250，begin=250,end=500
				线程3：c=250，begin=500,end=750
				线程4：c=250，begin=750,end=1000
	*/
	int c = cCount / tCount;
	int begin = (id - 1)*c;
	int end = id*c;

	for (int n = begin; n < end; ++n) //创建客户端
	{
		client[n] = new EasyTcpClient;
	}
	for (int n = begin; n < end; ++n) //让每个客户端连接服务器
	{
		client[n]->ConnectServer("192.168.0.105", 4567);
	}
	printf("Thread<%d>,Connect=<begin=%d, end=%d>\n", id, (begin + 1), end);

	
	//将readyCount，然后判断readyCount是否达到了tCount
	//如果没有，说明所有的线程还没有准备好，那么就等待所有线程都准备好一起返回发送数据
	readyCount++;
	while (readyCount < tCount)
	{
		std::chrono::microseconds t(10);
		std::this_thread::sleep_for(t);
	}

	//这里定义为数组，可以随根据需求修改客户端单次发送给服务端的数据包数量
	const int nNum = 10;
	Login login[nNum];
	for (int n = 0; n < nNum; ++n)
	{
		strcpy(login[n].userName, "dongshao");
		strcpy(login[n].PassWord, "123456");
	}
	//在外面定义nLen，就不用每次在for循环中SendData时还要去sizeof计算一下login的大小
	int nLen = sizeof(login);
	//循环向服务端发送消息

	while (g_bRun)
	{
		for (int n = begin; n < end; ++n)
		{
			if (client[n]->SendData(login, nLen) != SOCKET_ERROR)
			{
				sendCount++;
			}
			client[n]->Onrun();
		}
		
	}

	//关闭客户端
	for (int n = begin; n < end; ++n)
	{
		client[n]->CloseSocket();
		delete client[n];
	}

	printf("thread:all clients close the connection!\n");
}