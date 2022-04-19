//
//  CMIO_DP_Sample_PlugInInterface.c
//  Sample Plugin
//
//  Created by Tamás Lustyik on 2018. 11. 28..
//  实例化插件

#include "CMIO_DP_Sample_PlugIn.h"

// Public Utility Includes
#include "CMIODebugMacros.h"

// CA Public Utility Includes
#include "CAAutoDisposer.h"
#include "CAException.h"
#include "CACFString.h"

// System Includes
#include <CoreMediaIO/CMIOHardwarePlugin.h>
#include <IOKit/IOMessage.h>
#include <servers/bootstrap.h>

#warning demo中插件和助手之间通信使用的事苹果过时方案bootstrap，现已被XPC取代，后期可以优化

extern "C"
{
	// 实例化插件
	void* AppleCMIODPSampleNewPlugIn(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID);

    // 实例化插件
	void* AppleCMIODPSampleNewPlugIn(CFAllocatorRef allocator, CFUUIDRef requestedTypeUUID)
	{
		if (not CFEqual(requestedTypeUUID, kCMIOHardwarePlugInTypeID))
			return 0;
		
		try
		{
            // 在继续之前，请确保 SampleAssistant 进程已向 Mach 的引导服务注册。 通常，这将通过适当的
            // 在 /Library/LaunchDaemons 中配置 plist，但如果这样做了，那么进程将归 root 所有，从而使调试过程复杂化。 因此，如果发生
            // plist 丢失（大多数调试工作都会出现这种情况）现在尝试注册 SampleAssistant。 如果已经注册，它将优雅地失败。
			mach_port_t assistantServicePort;
			name_t assistantServiceName = "com.apple.cmio.DPA.Sample";
            // bootstrap方案查询助手服务，主要用于插件与助手的通信
			kern_return_t err = bootstrap_look_up(bootstrap_port, assistantServiceName, &assistantServicePort);
            // 查询不到助手服务，手动创建助手服务
			if (BOOTSTRAP_SUCCESS != err)
			{
                // 创建位于“/Library/CoreMediaIO/Plug-Ins/DAL/Sample.plugin/Contents/Resources/SampleAssistant”的 SampleAssistant的URL
				CACFURL assistantURL(CFURLCreateWithFileSystemPath(NULL, CFSTR("/Library/CoreMediaIO/Plug-Ins/DAL/Sample.plugin/Contents/Resources/SampleAssistant"), kCFURLPOSIXPathStyle, false));
				ThrowIf(not assistantURL.IsValid(), CAException(-1), "AppleCMIODPSampleNewPlugIn: unable to create URL for the SampleAssistant");

				// 获取SampleAssistant的绝对路径的文件系统表示的最大大小
				CFIndex length = CFStringGetMaximumSizeOfFileSystemRepresentation(CACFString(CFURLCopyFileSystemPath(CACFURL(CFURLCopyAbsoluteURL(assistantURL.GetCFObject())).GetCFObject(), kCFURLPOSIXPathStyle)).GetCFString());

				CAAutoFree<char> path(length);
				(void) CFURLGetFileSystemRepresentation(assistantURL.GetCFObject(), true, reinterpret_cast<UInt8*>(path.get()), length);

				mach_port_t assistantServerPort;
                // 创建assistantServer服务，主要用于插件和助手通信
				err = bootstrap_create_server(bootstrap_port, path, getuid(), true, &assistantServerPort);
				ThrowIf(BOOTSTRAP_SUCCESS != err, CAException(err), "AppleCMIODPSampleNewPlugIn: couldn't create server");
				
                // 再次确认assistantServer服务是否开启
				err = bootstrap_check_in(assistantServerPort, assistantServiceName, &assistantServicePort);

				// 确认assistantServer服务开启后不在需要服务端口，释放端口
				(void) mach_port_deallocate(mach_task_self(), assistantServerPort);

				// Make sure the call to bootstrap_create_service() succeeded
				ThrowIf(BOOTSTRAP_SUCCESS != err, CAException(err), "AppleCMIODPSampleNewPlugIn: couldn't create SampleAssistant service port");
			}

            // 确认assistantServer服务开启后不在需要服务端口，释放端口
			(void) mach_port_deallocate(mach_task_self(), assistantServicePort);

            // 实例化插件
			CMIO::DP::Sample::PlugIn* plugIn = new CMIO::DP::Sample::PlugIn(requestedTypeUUID, assistantServiceName);
            
            // 实例话plugIn对象引用计数+1，避免plugIn对象被提前释放，内存管理相关
			plugIn->Retain();
            
            // TODO:GetInterface作用？
			return plugIn->GetInterface();
		}
		catch (...)
		{
			return NULL;
		}
	}
}

