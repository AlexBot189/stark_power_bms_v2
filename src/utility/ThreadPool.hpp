#ifndef THREAD_POOL_H_0518
#define THREAD_POOL_H_0518

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <iostream>
#include <log_helper/LogHelper.h>

class ThreadPool
{
public:
	explicit ThreadPool(const size_t &count) : work_(ioService_)
	{
		ECO_INFO("Create threadPool, size = %d", count);
		for (size_t i = 0; i < count; ++i)
		{
			workers_.create_thread(boost::bind(&boost::asio::io_service::run, &ioService_));
		}
	}

	~ThreadPool()
	{
		ioService_.stop();
		workers_.join_all();
	}

	void Stop()
	{
		workers_.interrupt_all();
	}

	///< Add new work item to the pool.
	template <class F>
	void Enqueue(F f)
	{
		ioService_.post(f);
	}

private:
	boost::thread_group workers_;
	boost::asio::io_service ioService_;
	boost::asio::io_service::work work_;
};

#endif
