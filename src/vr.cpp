//module;
//
//#include <cstring>
//
//#include <vulkan/vulkan.h>
//
//#define XR_USE_GRAPHICS_API_VULKAN
//#include <openxr/openxr.h>
//#include <openxr/openxr_platform.h>
//
//#include "core/Console.h"
//
//module vr;
//
//import std;
//
//namespace vr
//{
//	namespace openxr
//	{
//		class Context
//		{
//		public:
//			XrInstance instance;
//			XrSystemId systemId;
//
//			bool SupportsViewConfigurationType(XrViewConfigurationType type)
//			{
//				uint32_t output = 0;
//				XrResult result = ::xrEnumerateViewConfigurations(instance, systemId, 0, &output, nullptr);
//				if (result != XR_SUCCESS)
//					return false;
//
//				std::vector<XrViewConfigurationType> configurationTypes(output);
//				result = ::xrEnumerateViewConfigurations(instance, systemId, output, &output, configurationTypes.data());
//				if (result != XR_SUCCESS)
//					return false;
//
//				for (int i = 0; i < configurationTypes.size(); i++)
//				{
//					if (configurationTypes[i] == type)
//						return true;
//				}
//				return false;
//			}
//
//			std::expected<std::vector<XrViewConfigurationView>, XrResult> GetConfigViews(XrViewConfigurationType type) // assumes that the type is supported
//			{
//				uint32_t output = 0;
//				XrResult result = ::xrEnumerateViewConfigurationViews(instance, systemId, type, 0, &output, nullptr);
//				if (result != XR_SUCCESS)
//					return std::unexpected(result);
//
//				
//				XrViewConfigurationView base{};
//				base.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
//
//				std::vector<XrViewConfigurationView> views(output, base);
//
//				result = ::xrEnumerateViewConfigurationViews(instance, systemId, type, output, &output, views.data());
//				if (result != XR_SUCCESS)
//					return std::unexpected(result);
//
//				return views;
//			}
//
//			template<typename T> bool LoadFunction(T& ptr, const std::string_view& name)
//			{
//				return ::xrGetInstanceProcAddr(instance, name.data(), reinterpret_cast<PFN_xrVoidFunction*>(&ptr)) == XR_SUCCESS;
//			}
//		};
//	}
//
//	constexpr XrViewConfigurationType CONFIG_TYPE = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
//
//	std::unique_ptr<openxr::Context> context;
//
//	bool CreateInstance()
//	{
//		std::vector<const char*> instExtensions =
//		{
//			"XR_KHR_vulkan_enable",
//		};
//
//		XrApplicationInfo info{};
//		strcpy_s(info.applicationName, "Halesia");
//		strcpy_s(info.engineName, "Halesia");
//		info.apiVersion = XR_CURRENT_API_VERSION;
//		info.engineVersion = 0;
//
//		XrInstanceCreateInfo instanceInfo{};
//		instanceInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
//		instanceInfo.applicationInfo = info;
//		instanceInfo.enabledApiLayerCount = 0;
//		instanceInfo.enabledExtensionCount = static_cast<std::uint32_t>(instExtensions.size());
//		instanceInfo.enabledExtensionNames = instExtensions.data();
//
//		XrResult result = ::xrCreateInstance(&instanceInfo, &context->instance);
//		return result == XR_SUCCESS;
//	}
//
//	bool GetSystemId()
//	{
//		if (!context)
//			return false;
//
//		XrSystemGetInfo info{};
//		info.type = XR_TYPE_SYSTEM_GET_INFO;
//		info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
//
//		XrResult result = xrGetSystem(context->instance, &info, &context->systemId);
//		if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
//		{
//			Console::WriteWarning("A head mounted display was requested, but is not avaible");
//			return false;
//		}
//	}
//
//	PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR;
//
//	void Init()
//	{
//		context = std::make_unique<openxr::Context>();
//
//		if (!CreateInstance())
//		{
//			context.reset(nullptr);
//			return;
//		}
//
//		if (!GetSystemId())
//			return;
//
//		if (!context->LoadFunction(xrGetVulkanInstanceExtensionsKHR, "xrGetVulkanInstanceExtensionsKHR"))
//		{
//			context.reset(nullptr);
//			return;
//		}
//	}
//
//	void Destroy()
//	{
//		::xrDestroyInstance(context->instance);
//		context.reset(nullptr);
//	}
//
//	Status GetStatus()
//	{
//		if (context == nullptr)
//			return Status::NoRuntime;
//		else if (context->systemId == 0)
//			return Status::NoDevice;
//		else
//			return Status::Good;
//	}
//
//	namespace graphics
//	{
//		std::expected<std::array<EyeDimension, 2>, Error> GetEyeDimensions()
//		{
//			if (context == nullptr)
//				return std::unexpected(Error::EarlierFailure);
//
//			if (!context->SupportsViewConfigurationType(CONFIG_TYPE))
//				return std::unexpected(Error::NotAvailable);
//
//			std::expected<std::vector<XrViewConfigurationView>, XrResult> eViews = context->GetConfigViews(CONFIG_TYPE);
//			if (!eViews.has_value())
//				return std::unexpected(Error::NotAvailable);
//
//			std::vector<XrViewConfigurationView>& views = *eViews;
//
//			if (views.size() == 1)
//				return std::unexpected(Error::NotAvailable);
//
//			if (views.size() != 2)
//				Console::WriteWarning("display has {} views, but expected 2", views.size());
//
//			std::array<EyeDimension, 2> ret{};
//			for (std::size_t i = 0; i < ret.size(); i++)
//			{
//				EyeDimension& dst = ret[i];
//				XrViewConfigurationView& view = views[i];
//
//				dst.width = view.recommendedImageRectWidth;
//				dst.height = view.recommendedImageRectHeight;
//				dst.sampleCount = view.recommendedSwapchainSampleCount;
//			}
//			return ret;
//		}
//
//		std::vector<std::string> GetInstanceExtensions()
//		{
//			std::vector<std::string> ret;
//			
//			std::uint32_t output = 0;
//			XrResult res = xrGetVulkanInstanceExtensionsKHR(context->instance, context->systemId, 0, &output, nullptr);
//			if (res != XR_SUCCESS)
//				return ret;
//
//			//std::vector<char> raw(output);
//			std::string raw;
//			raw.resize(output);
//			xrGetVulkanInstanceExtensionsKHR(context->instance, context->systemId, output, &output, raw.data());
//
//			std::size_t start = 0;
//			for (std::size_t count = raw.find_first_of(' ', start); count != std::string::npos; start += count)
//				ret.push_back(raw.substr(start, count));
//			
//			return ret;
//		}
//	}
//}