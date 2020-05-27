#ifndef __CELLTASK_HPP__
#define __CELLTASK_HPP__

#include <thread>
#include <mutex>
#include <list>
//#include <functional>

using namespace std;

//任务类型-基类
class CellTask
{
public:
	//执行任务
	virtual void doTask() = 0;
};

typedef std::shared_ptr<CellTask> CellTaskPtr;
//执行任务的服务类型
class CellTaskServer
{
public:
	//添加任务
	void addTask(CellTaskPtr& task)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_tasksBuff.push_back(task);
	}

	//启动服务
	void Start()
	{
		std::thread t(std::mem_fn(&CellTaskServer::OnRun), this);
		t.detach();
	}
private:
	//执行任务
	void OnRun();
private:
	std::list<CellTaskPtr> _tasks;     //任务数据链表
	std::list<CellTaskPtr> _tasksBuff; //任务数据缓冲区链表
	std::mutex _mutex;               //互斥锁
};

void CellTaskServer::OnRun()
{
	while (true)
	{
		//如果_tasksBuff不为空，那么将任务放入任务数据链表
		if (!_tasksBuff.empty())
		{
			std::lock_guard<std::mutex> lock(_mutex);
			for (auto pTask : _tasksBuff)
			{
				_tasks.push_back(pTask);
			}
			_tasksBuff.clear();
		}

		//如果当前没有任务，休眠1毫秒继续循环
		if (_tasks.empty())
		{
			std::chrono::milliseconds t(1);
			std::this_thread::sleep_for(t);
			continue;
		}

		//如果有任务，执行任务
		for (auto pTask : _tasks)
		{
			pTask->doTask();
		}
		_tasks.clear();
	}
}
#endif