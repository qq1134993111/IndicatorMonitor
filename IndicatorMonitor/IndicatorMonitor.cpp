#include "IndicatorMonitor.h"

#include <stdio.h>
#include <map>
#include <unordered_set>
#include<mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

#include "boost/filesystem.hpp"
#include <boost/property_tree/json_parser.hpp>

#include "general_processor.h"



struct DefaultFileMonitorSinker::Impl
{
public:
	Impl()
	{

	}
	~Impl()
	{
		Uninit();
	}

	bool Init(const boost::filesystem::path& file_dir, const std::string& app_name)
	{
		file_dir_ = boost::filesystem::weakly_canonical(file_dir);

		if (!boost::filesystem::is_directory(file_dir_))
		{
			return false;
		}

		if (!boost::filesystem::exists(file_dir_))
		{
			boost::system::error_code ec;
			boost::filesystem::create_directories(file_dir_, ec);
			if (!ec)
			{
				return false;
			}
		}
		app_name_ = app_name;

		auto now = std::chrono::system_clock::now();
		current_date_ = GetDate(now);
		file_name_ = "ind_" + app_name_ + "_" + GetDateStr(now) + ".ind";
		std::string file_full_path = (file_dir_ / file_name_).string();
		fp_ = fopen(file_full_path.c_str(), "a+");
		if (fp_ == nullptr)
		{
			return false;
		}

		return true;
	}

	void Uninit()
	{
		if (fp_ != nullptr)
		{
			fclose(fp_);
			fp_ = nullptr;
		}
	}

	void Process(int64_t count_id, uint64_t time_ticks, const boost::property_tree::ptree& content)
	{
		if (fp_ == nullptr)
			return;

		std::chrono::system_clock::time_point now{std::chrono::system_clock::duration(time_ticks)};
		//auto now = std::chrono::system_clock::now();
		int32_t  date = GetDate(static_cast<std::chrono::system_clock::time_point>(now));
		if (date != current_date_)
		{
			fclose(fp_);

			current_date_ = date;
			file_name_ = "ind_" + app_name_ + "_" + GetDateStr(static_cast<std::chrono::system_clock::time_point>(now)) + ".ind";
			std::string file_full_path = (file_dir_ / file_name_).string();
			fp_ = fopen(file_full_path.c_str(), "w+");

			if (fp_ == nullptr)
				return;
		}

		oss_.clear();
		oss_.str("");

		boost::property_tree::json_parser::write_json(oss_, content, false);

		fprintf(fp_, "@ %s %lld Indicator %s", GetDateTimeMicro(static_cast<std::chrono::system_clock::time_point>(now)).c_str(), count_id, oss_.str().c_str());
		fflush(fp_);
	}

private:

	int32_t GetDate(const std::chrono::system_clock::time_point& now = std::chrono::system_clock::now())
	{
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		return GetDate(time);
	}

	int32_t GetDate(const std::time_t& time = time(nullptr))
	{
		std::tm tm = *std::localtime(&time);
		return GetDate(tm);
	}

	int32_t GetDate(const std::tm& tm)
	{
		int year = tm.tm_year + 1900; // 年份为1900年起至今的年数
		int month = tm.tm_mon + 1; // 月份为0-11，需要加上1
		int day = tm.tm_mday; // 月中的日期，1-31

		return year * 10000 + month * 100 + day;
	}

	std::string GetDateStr(const std::chrono::system_clock::time_point& now = std::chrono::system_clock::now())
	{
		//auto now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		return GetDateStr(time);
	}

	std::string GetDateStr(const std::time_t& time = time(nullptr))
	{
		std::tm tm = *std::localtime(&time);
		return GetDateStr(tm);
	}

	std::string GetDateStr(const std::tm& tm)
	{
		int year = tm.tm_year + 1900; // 年份为1900年起至今的年数
		int month = tm.tm_mon + 1; // 月份为0-11，需要加上1
		int day = tm.tm_mday; // 月中的日期，1-31

		std::stringstream ss;
		ss << std::setfill('0') << std::setw(4) << year << "-" << std::setw(2) << month << "-" << std::setw(2) << day;
		return ss.str();
	}

	std::string GetCurrentDateTime()
	{
		std::time_t now = std::time(nullptr);
		std::tm tm = *std::localtime(&now);
		int year = tm.tm_year + 1900; // 年份为1900年起至今的年数
		int month = tm.tm_mon + 1; // 月份为0-11，需要加上1
		int day = tm.tm_mday; // 月中的日期，1-31
		int hour = tm.tm_hour; // 时，0-23
		int minute = tm.tm_min; // 分，0-59
		int second = tm.tm_sec; // 秒，0-59
		std::stringstream ss;
		ss << std::setfill('0') << std::setw(4) << year << "-" << std::setw(2) << month << "-" << std::setw(2) << day << " " << std::setw(2) << hour << ":" << std::setw(2) << minute << ":" << std::setw(2) << second;
		return ss.str();
	}

	std::string GetDateTimeMicro(const std::chrono::system_clock::time_point& now = std::chrono::system_clock::now())
	{
		//auto now = std::chrono::system_clock::now();
		std::time_t time = std::chrono::system_clock::to_time_t(now);
		std::tm tm = *std::localtime(&time);
		auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
		std::stringstream ss;
		ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(6) << us.count();
		return ss.str();
	}

private:

	boost::filesystem::path file_dir_;
	std::string app_name_;
	std::string file_name_;
	int32_t current_date_;

	FILE* fp_ = nullptr;
	std::ostringstream oss_;
};


DefaultFileMonitorSinker::DefaultFileMonitorSinker()
{
	impl_.reset(new Impl());
}

DefaultFileMonitorSinker::~DefaultFileMonitorSinker()
{
	impl_.reset();
}

bool DefaultFileMonitorSinker::Init(const boost::filesystem::path& file_dir, const std::string& app_name)
{
	return impl_->Init(file_dir, app_name);
}

void DefaultFileMonitorSinker::Process(int64_t count_id, uint64_t time_ticks, const boost::property_tree::ptree& content)
{
	impl_->Process(count_id, time_ticks, content);
}


class MonitorService
{
public:
	struct ObjecctNode
	{
		MonitorOps  monitor_ops_;
		//std::string object_name_;
	};
	using ObjecctNodeMap = std::map<std::string, ObjecctNode>;

	struct ClassNode
	{
		//std::string  class_name_;
		ObjecctNodeMap  object_map_;
		std::weak_ptr<boost::asio::steady_timer> timer_;
	};
	using ClassNodeMap = std::map<std::string, ClassNode>;

	MonitorService()
	{
	}
	~MonitorService()
	{
		Stop();
	}
public:
	bool Start()
	{

		return 	processor_.Start("MonitorService", 1);
	}

	bool Stop()
	{
		processor_.Stop();

		return true;
	}

	bool IsStart()
	{
		return processor_.IsStart();
	}

	void AddSinker(std::shared_ptr<IMonitorSinker> sinker)
	{
		std::unique_lock<std::mutex> lc(mtx_);
		sinker_set_.insert(sinker);
	}

	void RemoveSinker(std::shared_ptr<IMonitorSinker> sinker)
	{
		std::unique_lock<std::mutex> lc(mtx_);
		sinker_set_.erase(sinker);
	}

	bool RegisterObject(const std::string& class_name, const std::string& object_name, MonitorOps& ops)
	{
		std::unique_lock<std::mutex> lc(mtx_);

		class_node_map_[class_name].object_map_[object_name].monitor_ops_ = ops;

		auto sp_timer = class_node_map_[class_name].timer_.lock();
		if (sp_timer)
		{
			processor_.CancelTimer(nullptr, sp_timer);
		}

		class_node_map_[class_name].timer_ = processor_.AddTimer(std::chrono::milliseconds(ops.collection_interval_mill),
			[class_name, this]()->bool {
				DoOnCollectionIndictor(class_name);
				return true;
			}
		);

		return true;
	}

	bool  UnregisterObject(const std::string& class_name, const std::string& object_name)
	{
		std::unique_lock<std::mutex> lc(mtx_);

		auto  it = class_node_map_.find(class_name);
		if (it != class_node_map_.end())
		{
			it->second.object_map_.erase(object_name);
			if (it->second.object_map_.empty())
			{
				auto timer = it->second.timer_;
				class_node_map_.erase(it);
				lc.unlock();

				processor_.CancelTimer(nullptr, timer);
			}
			return true;
		}

		return false;
	}

	bool HasObjiect()
	{
		std::unique_lock<std::mutex> lc(mtx_);

		int size = class_node_map_.size();

		return size;
	}
protected:
	void DoOnCollectionIndictor(const std::string& class_name)
	{
		std::unique_lock<std::mutex> lc(mtx_);

		ClassNode* p_class_node = nullptr;
		auto it = class_node_map_.find(class_name);
		if (it != class_node_map_.end())
		{
			p_class_node = &(it->second);
		}
		else
		{
			return;
		}

		boost::property_tree::ptree  root_tree;
		root_tree.put("class_name", class_name);
		boost::property_tree::ptree& objects_tree = root_tree.add_child("class_objects", boost::property_tree::ptree());
		for (auto& apair : p_class_node->object_map_)
		{
			auto& object_name = apair.first;
			auto& object_node = apair.second;

			boost::property_tree::ptree obj_tree;
			obj_tree.put("object_name", object_name);


			if (object_node.monitor_ops_.on_collection_indictor)
			{

				if (object_node.monitor_ops_.on_collection_indictor(obj_tree))
				{
					//obj_tree.put("object_collection_status", "successful");
				}
				else
				{
					obj_tree.put("object_collection_status", "failure");
				}

				objects_tree.push_back(std::make_pair("", obj_tree));
			}
			else
			{
				continue;
			}

		}

		auto sinkers = sinker_set_;
		lc.unlock();

		auto count_id = ++count_;
		auto now = std::chrono::system_clock::now();
		auto time_ticks = now.time_since_epoch().count();

		for (auto& sinker : sinkers)
		{
			sinker->Process(count_id, time_ticks, root_tree);
		}
	}
private:
	ClassNodeMap class_node_map_;
	std::mutex mtx_;

	GeneralProcessor  processor_;
	int64_t count_ = 0;
	std::unordered_multiset<std::shared_ptr<IMonitorSinker>> sinker_set_;
	//std::shared_ptr<IMonitorSinker> default_sinker_;
};


struct MonitorStruct
{
	std::unique_ptr<MonitorService> service_;
	std::mutex  mtx_;
	MonitorStruct()
	{
		service_.reset(new MonitorService);
	}
};

MonitorStruct& GetDefaultMonitorInfo()
{
	static MonitorStruct info;

	return info;
}

bool IndicatorMonitor::RegisterObject(const std::string& class_name, const std::string& object_name, MonitorOps& ops)
{
	if (class_name.empty() || object_name.empty())
		return false;

	auto& info = GetDefaultMonitorInfo();
	if (info.service_->RegisterObject(class_name, object_name, ops))
	{
		if (info.service_->IsStart())
		{
			return true;
		}

		std::unique_lock<std::mutex> lc(info.mtx_);
		if (!info.service_->IsStart())
		{
			if (info.service_->Start())
			{
				return true;
			}
		}


		return true;

	}

	return false;
}

void IndicatorMonitor::UnregisterObject(const std::string& class_name, const std::string& object_name)
{
	auto& info = GetDefaultMonitorInfo();

	if (info.service_->UnregisterObject(class_name, object_name))
	{
		std::unique_lock<std::mutex> lc(info.mtx_);
		if (!info.service_->HasObjiect())
		{
			info.service_->Stop();
		}
	}
}

bool IndicatorMonitor::AddSinker(std::shared_ptr<IMonitorSinker> sinker)
{
	if (sinker == nullptr)
		return false;

	auto& info = GetDefaultMonitorInfo();
	info.service_->AddSinker(sinker);
	return true;
}

bool IndicatorMonitor::RemoveSinker(std::shared_ptr<IMonitorSinker> sinker)
{
	if (sinker == nullptr)
		return false;

	auto& info = GetDefaultMonitorInfo();
	info.service_->RemoveSinker(sinker);
	return true;
}
