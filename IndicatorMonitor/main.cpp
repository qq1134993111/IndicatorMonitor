// IndicatorMonitor.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "IndicatorMonitor.h"
#include "general_processor.h"

#include <thread>
#include <chrono>

int main()
{
    /*
    GeneralProcessor processor;
   
	auto timer = processor.AddTimer(std::chrono::seconds(1), []()->bool {
		printf("hello world 1\n");
		return true;
		});

    processor.Start();
    //processor.Post(nullptr, []() {  printf("hello world\n"); });


    std::this_thread::sleep_for(std::chrono::seconds(20));
    processor.CancelTimer(nullptr,timer);
    */

    auto sinker=std::make_shared<DefaultFileMonitorSinker>();
    sinker->Init(".", "test");

    IndicatorMonitor::AddSinker(sinker);
    if (1)
    {
        MonitorOps ops;
        ops.on_collection_indictor = [](boost::property_tree::ptree& indictor)->bool
        {
            indictor.put("hello", "world");
            return true;
        };
        IndicatorMonitor::RegisterObject("test", "hello", ops);
        std::this_thread::sleep_for(std::chrono::minutes(1));
        IndicatorMonitor::UnregisterObject("test","hello");
        
    }

    if (1)
    {
		MonitorOps ops;
		ops.on_collection_indictor = [](boost::property_tree::ptree& indictor)->bool
		{
			indictor.put("hello2", "world2");
			return true;
		};
		IndicatorMonitor::RegisterObject("test", "hello2", ops);
		std::this_thread::sleep_for(std::chrono::minutes(1));
		IndicatorMonitor::UnregisterObject("test", "hello2");

    }

   

    std::cout << "Hello World!\n";
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
