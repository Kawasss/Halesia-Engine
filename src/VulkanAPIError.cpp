#include <vulkan/vk_enum_string_helper.h>
#include <sstream>
#include <vector>

#include "renderer/VulkanAPIError.h"
#include "renderer/Vulkan.h"

VulkanAPIError::VulkanAPIError(std::string message, VkResult result, std::source_location location)
{
    std::stringstream stream; // result can be VK_SUCCESS for functions that dont use a vulkan functions, i.e. looking for a physical device but there are none that fit the bill
    stream << message << "\n\n";
    if (result != VK_SUCCESS)
        stream << string_VkResult(result) << ' ';

    stream << "from " << location.function_name() << " at line " << location.line() << " in " << location.file_name();

#ifdef _DEBUG
    uint32_t count = 0;
    vkGetQueueCheckpointDataNV(Vulkan::GetContext().graphicsQueue, &count, nullptr);
    if (count > 0)
    {
        VkCheckpointDataNV base{};
        base.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;

        std::vector<VkCheckpointDataNV> checkpoints(count, base);
        vkGetQueueCheckpointDataNV(Vulkan::GetContext().graphicsQueue, &count, checkpoints.data());

        stream << "\n\nCheckpoints:\n";
        for (VkCheckpointDataNV& data : checkpoints)
        {
            //const char* msg = static_cast<const char*>(data.pCheckpointMarker);
            //size_t msgLength = strnlen_s(msg, 128); // if the message is not a valid string (null terminator could not be found) then the marker is an integer value

            stream << string_VkPipelineStageFlagBits(data.stage) << ", ";
            /*if (msgLength >= 128)
                stream << reinterpret_cast<uint64_t>(data.pCheckpointMarker);
            else
                stream << msg;*/
            stream << "\n\n";
        }
    }
#endif
    this->message = stream.str();
}