#pragma once
#include "boost/asio.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/thread.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <deque>

#include <stdint.h>

class IoServiceThread
{
private:
	IoServiceThread(const IoServiceThread&) = delete;
	IoServiceThread& operator =(const IoServiceThread&) = delete;
	IoServiceThread(IoServiceThread&&) = delete;
	IoServiceThread& operator =(IoServiceThread&&) = delete;
public:
	IoServiceThread(boost::asio::io_service& ios, const std::string& thread_name = "IoServiceThread")
		: ios_(ios)
		, thread_name_(thread_name)
		, is_start_(false)
		, is_stop_(true)
	{
	}

	~IoServiceThread()
	{
		Stop();
	}

	bool Start()
	{
		std::unique_lock<std::mutex> lk(mtx_);
		if (is_start_)
			return true;

		try
		{
			thread_ = std::thread([this]() {OnRun(this->thread_name_); });
		}
		catch (const std::system_error& e)
		{
			printf("create thread failed:%d,%s\n", e.code().value(), e.what());

			return false;
		}

		cv_.wait(lk, [this] {return is_start_; });

		return true;

	}

	void Stop(bool wait = true)
	{
		std::unique_lock<std::mutex> lk(mtx_);
		if (is_stop_)
		{
			if (thread_.joinable())
			{
				thread_.join();
			}

			is_start_ = false;

			return;
		}

		is_stop_ = true;

		if (wait)
		{
			if (thread_.joinable())
			{
				thread_.join();
			}

			is_start_ = false;
		}

	}

public:
	//设置线程优先级
	void SetSchedParam(int policy, int priority)
	{
#ifndef _WIN32
		// policy:线程的调度有三种策略：SCHED_OTHER、SCHED_RR和SCHED_FIFO
		// priority: 0到99 

		auto handle = thread_.native_handle();
		sched_param param = { priority % 100 };
		int ret = pthread_setschedparam(handle, policy, &param);
		if (0 != ret)
		{
			printf("warning : set thread sched param failed, error code is [%d],[%s]", ret, strerror(ret));
		}
#else 
		/*
  priority
  THREAD_MODE_BACKGROUND_BEGIN	0x00010000	将线程的调度模式设置为后台模式。在后台模式下，线程的基本优先级将降低，以便前台模式下的线程可以更好地使用CPU时间。
  THREAD_MODE_BACKGROUND_END	0x00020000	将线程的调度模式恢复为标准模式。
  THREAD_PRIORITY_ABOVE_NORMAL	1	指示比正常情况下稍高一点的优先级。
  THREAD_PRIORITY_BELOW_NORMAL	-1	指示比正常情况下稍低一点的优先级。
  THREAD_PRIORITY_HIGHEST	2	指示最高优先级。
  THREAD_PRIORITY_IDLE	-15	指示最低优先级。
  THREAD_PRIORITY_LOWEST	-2	指示比正常情况下低得多的优先级。
  THREAD_PRIORITY_NORMAL	0	指示正常优先级。
  THREAD_PRIORITY_TIME_CRITICAL	15	指示最高实时优先级。
		*/

		auto handle = thread_.native_handle();
		//policy 为1开启，为0关闭
		// 设置线程的优先级提升状态
		if (!SetThreadPriorityBoost(handle, (BOOL)policy))
		{
			printf("SetThreadPriorityBoost failed, error code: %d\n", GetLastError());
		}

		// 设置线程的基本优先级或调度模式
		if (!SetThreadPriority(handle, (int)priority))
		{
			printf("SetThreadPriority failed, error code: %d\n", GetLastError());
		}

#endif

	}
	//设置线程亲和性
	void SetAffinity(std::size_t index)
	{
#ifdef __linux__

		cpu_set_t mask;
		CPU_ZERO(&mask);
		CPU_SET(index % std::thread::hardware_concurrency(), &mask);

		auto handle = thread_.native_handle();
		int ret = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &mask);
		if (0 != ret)
		{
			printf("warning : set thread affinity failed, error code is [%d],[%s]\n", ret, strerror(ret));
		}
#else
		DWORD_PTR old_mask, new_mask;
		// 将每个线程绑定到一个CPU核上
		new_mask = static_cast<DWORD_PTR>(1) << (index % std::thread::hardware_concurrency());
		auto handle = thread_.native_handle();
		old_mask = SetThreadAffinityMask(handle, new_mask);
		if (old_mask == 0)
		{
			// 函数失败，打印错误信息
			printf("SetThreadAffinityMask failed for thread %lld, error code:%d\n", index, GetLastError());
		}
		else {
			// 函数成功，打印旧的亲和性掩码
			printf("SetThreadAffinityMask succeeded for thread %lld, old mask::%lld\n", index, old_mask);

		}

#endif
	}
protected:
	//设置线程名称
	void SetThreadName(const std::string& name)
	{
		const auto handle = thread_.native_handle();
#ifdef _WIN32
		// 获取ANSI字符串的长度
		std::size_t len = name.size();
		// 分配足够的空间来存储Unicode字符串
		std::wstring wstr(len, L'\0');
		// 转换ANSI字符串到Unicode字符串
		MultiByteToWideChar(CP_ACP, 0, name.c_str(), (int)len, &wstr[0], (int)len);
		// 调用SetThreadDescription函数
		HRESULT hr = SetThreadDescription(handle, wstr.c_str());
		if (SUCCEEDED(hr)) {
			// 函数成功，打印成功信息
			printf("SetThreadDescription succeeded.\n");
		}
		else {
			// 函数失败，打印错误信息
			printf("SetThreadDescription failed, error code: %d\n", hr);
		}
#else
		int rc = pthread_setname_np(handle, name.substr(0, 15));
		if (rc != 0) {
			// 函数失败，打印错误信息
			printf("pthread_setname_np error:[%d],[%s]\n", rc, strerror(rc));

		}
		else {
			// 函数成功，打印成功信息
			printf("pthread_setname_np succeeded.\n");
		}
#endif
	}

	void OnRun(std::string& thread_name)
	{
		{
			std::lock_guard<std::mutex> lk(mtx_);
			is_start_ = true;
			is_stop_ = false;
			SetThreadName(thread_name_);
		}
		cv_.notify_one();

		boost::system::error_code ec;

		while (!is_stop_) {
			try
			{
				ios_.run_one(ec);
			}
			catch (const std::exception& e)
			{
				printf("boost::asio::io_service run_one exception %s\n", e.what());
			}
			catch (...)
			{
				printf("boost::asio::io_service run_one exception\n");
			}

		}

	}
private:
	boost::asio::io_service& ios_;
	std::string thread_name_;
	bool is_start_;
	std::atomic<bool> is_stop_;
	std::thread thread_;
	std::mutex mtx_;
	std::condition_variable cv_;
};


class GeneralProcessor
{
private:
	GeneralProcessor(const GeneralProcessor&) = delete;
	GeneralProcessor& operator =(const GeneralProcessor&) = delete;
	GeneralProcessor(GeneralProcessor&&) = delete;
	GeneralProcessor& operator =(GeneralProcessor&&) = delete;
public:
	GeneralProcessor()
		: ios_()
		, work_(new boost::asio::io_service::work(ios_))
		, strand_(ios_)
	{

	}

	GeneralProcessor(std::string name, size_t thread_number = 1, bool binding_core = false, int32_t binding_core_start_index = 0)
		: GeneralProcessor()
	{
		if (!Start(std::move(name), thread_number, binding_core, binding_core_start_index))
		{
			throw std::runtime_error("GeneralProcessor Construct failed, Start failed");
		}
	}

	~GeneralProcessor() { Stop(); }

	bool Start(std::string name = "GeneralProcessor", size_t thread_number = 1, bool binding_core = false, int32_t binding_core_start_index = 0)
	{

		bool start = false;
		if (is_start_.compare_exchange_strong(start, true))
		{
			name_ = std::move(name);
			binding_core_ = binding_core;
			binding_core_start_index_ = binding_core_start_index;

			for (index_ = 0; index_ < thread_number; index_++)
			{
				auto p_io_thread = CreateIoThread(name_ + std::to_string(index_));
				v_io_queue_.push_back(std::move(p_io_thread));
			}

			bool start_all = true;

			for (std::size_t i = 0; i < v_io_queue_.size(); i++)
			{
				auto& p_io_thread = v_io_queue_[i];
				if (!p_io_thread->Start())
				{
					start_all = false;
					break;
				}

				if (binding_core_)
				{
					p_io_thread->SetAffinity(binding_core_start_index_ + i);
				}
#ifndef _WIN32				
				p_io_thread->SetSchedParam(SCHED_FIFO, 99);
#else
				p_io_thread->SetSchedParam(true, THREAD_PRIORITY_TIME_CRITICAL);
#endif
			}

			if (!start_all)
			{
				v_io_queue_.clear();
				is_start_ = false;
				return false;
			}

			is_stop_ = false;
			return true;
		}

		return false;

	}

	void Stop()
	{
		bool stop = false;
		if (is_stop_.compare_exchange_strong(stop, true))
		{

			work_.reset();
			for (auto& io : v_io_queue_)
			{
				io->Stop(false);
			}

			v_io_queue_.clear();
			is_start_ = false;
		}

		return;
	}

	bool IsStart()
	{
		return is_start_;
	}

	int32_t AddThread(int32_t number)
	{
		if (!is_start_)
			return 0;

		std::unique_lock<std::mutex> lc(io_queue_mtx_);
		int count = 0;
		for (std::size_t i = 0; i < number; i++)
		{

			auto p_io_thread = CreateIoThread(name_ + std::to_string(index_));
			if (!p_io_thread->Start())
			{
				return false;
			}
			if (binding_core_)
			{
				p_io_thread->SetAffinity(binding_core_start_index_ + index_);
			}
#ifndef _WIN32				
			p_io_thread->SetSchedParam(SCHED_FIFO, 99);
#else
			p_io_thread->SetSchedParam(true, THREAD_PRIORITY_TIME_CRITICAL);
#endif

			v_io_queue_.push_back(std::move(p_io_thread));
			index_++;

			count++;
		}

		return count;
	}

	int32_t SubThread(int32_t number)
	{
		if (!is_start_)
			return 0;

		std::unique_lock<std::mutex> lc(io_queue_mtx_);

		int32_t count = 0;
		for (int i = 0; i < number; i++)
		{
			if (!v_io_queue_.empty())
			{
				v_io_queue_.back().reset();
				v_io_queue_.pop_back();
				index_--;
				count++;
			}
		}

		return count;
	}

	boost::asio::io_service::strand GenStrand()
	{
		boost::asio::io_service::strand strand(ios_);
		return strand;
	}

	template<typename Function>
	void Post(boost::asio::io_service::strand* p_strand, Function&& function)
	{
		if (p_strand == nullptr)
		{
			p_strand = &strand_;
		}
		p_strand->post(std::forward<Function>(function));
	}

	template<typename Function>
	void Dispatch(boost::asio::io_service::strand* p_strand, Function&& function)
	{
		if (p_strand == nullptr)
		{
			p_strand = &strand_;
		}
		p_strand->dispatch(std::forward<Function>(function));

		//auto handler = p_strand->wrap(std::forward<Function>(function));
		//handler();
	}

	template<typename FunctionType>
	auto Commit(boost::asio::io_service::strand* p_strand, FunctionType&& func) -> std::future<decltype(std::declval<FunctionType>()())>
	{
		if (p_strand == nullptr)
		{
			p_strand = &strand_;
		}

		using ResultType = decltype(std::declval<FunctionType>()());

		std::shared_ptr<std::packaged_task<ResultType()>> task = std::make_shared<std::packaged_task<ResultType()>>(std::forward<FunctionType>(func));
		std::future<ResultType> result(task->get_future());

		p_strand->post([task]() {(*task)(); });

		return result;
	}


	class TimerObject :public boost::asio::steady_timer
	{
		friend class GeneralProcessor;
	public:
		using boost::asio::steady_timer::steady_timer;

		std::size_t Stop()
		{
			stop_ = true;

			boost::system::error_code ec;
			auto cancel_count = cancel(ec);
			return cancel_count;
		}

		bool IsStopped()
		{
			return stop_;
		}
	private:
		bool stop_ = true;
	};

	template<class Rep, class Period, typename Function>
	std::weak_ptr<TimerObject> AddTimer(boost::asio::io_service::strand* p_strand, std::chrono::duration<Rep, Period>  duration, Function&& function)
	{
		if (p_strand == nullptr)
		{
			p_strand = &strand_;
		}

		return SetTimer(*p_strand, duration, std::forward<Function>(function), nullptr);
	}


	template<class Rep, class Period, class F, class... Args>
	std::weak_ptr<TimerObject> AddTimer(boost::asio::io_service::strand* p_strand, std::chrono::duration<Rep, Period> duration, F&& f, Args&&... args)
	{
		auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		return AddTimer(duration, std::move(func), p_strand);
	}

	std::size_t CancelTimer(const std::weak_ptr<TimerObject>& weak_timer)
	{
		auto sp = weak_timer.lock();
		if (sp)
		{
			return sp->Stop();
		}

		return 0;
	}

	template<typename Function>
	void Post(Function&& function)
	{
		ios_.post(std::forward<Function>(function));
	}
	template<typename Function>
	void Dispatch(Function&& function)
	{
		ios_.dispatch(std::forward<Function>(function));
	}

	template<typename FunctionType>
	auto Commit(const FunctionType& func) -> std::future<decltype(std::declval<FunctionType>()())>
	{
		using ResultType = decltype(std::declval<FunctionType>()());

		std::packaged_task<ResultType()> task(func);
		std::future<ResultType> result(task.get_future());
		ios_.post(std::move(task));
		return result;
	}

	template<class Rep, class Period, typename Function>
	std::weak_ptr<TimerObject> AddTimer(std::chrono::duration<Rep, Period>  duration, Function&& function)
	{
		return SetTimer(duration, std::forward<Function>(function), nullptr);
	}


	template<class Rep, class Period, class F, class... Args>
	std::weak_ptr<TimerObject> AddTimer(std::chrono::duration<Rep, Period> duration, F&& f, Args&&... args)
	{
		auto func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
		return AddTimer(duration, std::move(func));
	}

protected:
	std::unique_ptr<IoServiceThread> CreateIoThread(std::string thread_name)
	{
		std::unique_ptr<IoServiceThread> p(new IoServiceThread(ios_, thread_name));
		return p;
	}

	template<class Rep, class Period, typename Function>
	std::weak_ptr<TimerObject> SetTimer(std::chrono::duration<Rep, Period>  duration, Function&& function, std::shared_ptr<TimerObject> timer = nullptr)
	{
		if (timer == nullptr)
		{
			timer = std::make_shared<TimerObject>(ios_, duration);
			timer->stop_ = false;
		}
		else
		{
			timer->expires_from_now(duration);
		}

		timer->async_wait([this, duration, function = std::forward<Function>(function), timer](const boost::system::error_code& ec)
			{

				if (ec)return;

				if (!timer->IsStopped() && function())
				{
					SetTimer(duration, std::move(function), timer);
				}
			});


		return  timer;
	}


	template<class Rep, class Period, typename Function>
	std::weak_ptr<TimerObject> SetTimer(boost::asio::io_service::strand& my_strand, std::chrono::duration<Rep, Period>  duration, Function&& function, std::shared_ptr<TimerObject> timer = nullptr)
	{
		if (timer == nullptr)
		{
			timer = std::make_shared<TimerObject>(my_strand.context(), duration);
			timer->stop_ = false;
		}
		else
		{
			timer->expires_from_now(duration);
		}

		timer->async_wait(my_strand.wrap([this, &my_strand, duration, function = std::forward<Function>(function), timer](const boost::system::error_code& ec)
			{

				if (ec)return;

				if (!timer->IsStopped() && function())
				{
					SetTimer(my_strand, duration, std::move(function), timer);
				}
			}));


		return  timer;
	}

private:
	boost::asio::io_service ios_;
	std::unique_ptr<boost::asio::io_service::work> work_;
	boost::asio::io_service::strand strand_;
	std::deque<std::unique_ptr<IoServiceThread>> v_io_queue_;
	std::mutex io_queue_mtx_;

	std::atomic<bool> is_start_{false};
	std::atomic<bool> is_stop_{true};
	std::string name_;
	bool  binding_core_ = false;
	std::size_t binding_core_start_index_ = 0;
	std::size_t index_ = 0;

};