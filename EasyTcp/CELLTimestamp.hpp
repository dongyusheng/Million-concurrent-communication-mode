#ifndef _CELLTimestamp_hpp_
#define _CELLTimestamp_hpp_

#include <chrono>

using namespace std::chrono;

class CELLTimestamp
{
public:
	CELLTimestamp(){
		update();
	}

	//更新一下当前时间
	void update()
	{
		_begin = high_resolution_clock::now();
	}
	//获取当前秒
	double getElapsedSecond() {
		return getElapsedTimeInMicroSec() * 0.000001;
	}
	//获取毫秒
	double getElapsedTimeInMilliSec() {
		return getElapsedTimeInMicroSec() * 0.001;
	}
	//获取微秒
	long long getElapsedTimeInMicroSec() {
		//将当前时间减去开始时间：high_resolution_clock::now() - _begin
		//然后将结果转换为微秒microseconds，duration_cast用来强制转换
		return duration_cast<microseconds>(high_resolution_clock::now() - _begin).count();
	}
protected:
	time_point<high_resolution_clock> _begin;
};

#endif