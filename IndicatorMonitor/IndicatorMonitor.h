#pragma once
#include <functional>
#include "boost/property_tree/ptree.hpp"
#include "boost/filesystem.hpp"

struct MonitorOps
{
	uint32_t collection_interval_mill;
	std::function<bool(boost::property_tree::ptree& indictor)> on_collection_indictor;

	MonitorOps()
	{
		collection_interval_mill = 3000;
	}
};


struct IMonitorSinker
{
	virtual void Process(int64_t count_id, uint64_t time_ticks, const boost::property_tree::ptree& content) = 0;
};

class DefaultFileMonitorSinker :public IMonitorSinker
{
public:
	DefaultFileMonitorSinker();
	~DefaultFileMonitorSinker();
	bool Init(const boost::filesystem::path& file_dir, const std::string& app_name);
protected:
	virtual void Process(int64_t count_id, uint64_t time_ticks, const boost::property_tree::ptree& content) override;
private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

class IndicatorMonitor
{
public:
	static bool RegisterObject(const std::string& class_name, const std::string& object_name, MonitorOps& ops);
	static void UnregisterObject(const std::string& class_name, const std::string& object_name);

	static bool  AddSinker(std::shared_ptr<IMonitorSinker> sinker);
	static bool  RemoveSinker(std::shared_ptr<IMonitorSinker> sinker);

	//static int32_t  Start();
	//static int32_t  Stop();
};

