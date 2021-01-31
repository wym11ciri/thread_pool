#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <vector>
#include <queue>

class ThreadPool
{
public:
	using Task = std::function<void()>;

	explicit ThreadPool(int num): _thread_num(num), _is_running(false)
	{}

	~ThreadPool()
	{
		if (_is_running)
			stop();
	}

	void start()//开启进程 
	{
		_is_running = true;

		// start threads
		for (int i = 0; i < _thread_num; i++)
			_threads.emplace_back(std::thread(&ThreadPool::work, this));
	}

	void stop()
	{
		{
			// stop thread pool, should notify all threads to wake
			//任务调度需要加锁和互斥
                        //每次添加一个新的任务就唤醒一个线程
                        //线程池析构时先唤醒所有线程，然后终止线程回收资源
			std::unique_lock<std::mutex> lk(_mtx);
			_is_running = false;
			_cond.notify_all(); // must do this to avoid thread block
		}

		// terminate every thread job
		for (std::thread& t : _threads)
		{
			if (t.joinable())
				t.join();
		}
	}

	void appendTask(const Task& task)
	{
		if (_is_running)
		{
			std::unique_lock<std::mutex> lk(_mtx);//加锁
			_tasks.push(task);
			_cond.notify_one(); // wake a thread to to the task
		}
	}

private:
	void work()
	{
		printf("begin work thread: %d\n", std::this_thread::get_id());

		// every thread will compete to pick up task from the queue to do the task
		while (_is_running)
		{
			Task task;
			{
				//std::unique_lock对象以独占所有权的方式(unique owership)管理mutex对象的上锁和解锁操作，
				//即在unique_lock对象的声明周期内，
				//它所管理的锁对象会一直保持上锁状态；
				//而unique_lock的生命周期结束之后，它所管理的锁对象会被解锁。 
				std::unique_lock<std::mutex> lk(_mtx);//加锁 
				if (!_tasks.empty())
				{
					// if tasks not empty, 只要任务开始了就得执行完 
					// must finish the task whether thread pool is running or not
					task = _tasks.front();
					_tasks.pop(); // remove the task
				}
				else if (_is_running && _tasks.empty()) //budong 
					_cond.wait(lk);
			}

			if (task)
				task(); // do the task
		}

		printf("end work thread: %d\n", std::this_thread::get_id());
	}

public:
	// disable copy and assign construct
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool& other) = delete;

private:
	std::atomic_bool _is_running; // thread pool manager status
	std::mutex _mtx;
	std::condition_variable _cond;
	int _thread_num;
	std::vector<std::thread> _threads;
	std::queue<Task> _tasks;
};


#endif // !_THREAD_POOL_H_
