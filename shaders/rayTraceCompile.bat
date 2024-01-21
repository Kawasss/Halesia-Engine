@echo off
C:/VulkanSDK/1.3.261.1/Bin/glslc.exe uncompiled/gen.rgen --target-env=vulkan1.2 -o spirv/gen.rgen.spv
C:/VulkanSDK/1.3.261.1/Bin/glslc.exe uncompiled/hit.rchit --target-env=vulkan1.2 -o spirv/hit.rchit.spv
C:/VulkanSDK/1.3.261.1/Bin/glslc.exe uncompiled/miss.rmiss --target-env=vulkan1.2 -o spirv/miss.rmiss.spv
C:/VulkanSDK/1.3.261.1/Bin/glslc.exe uncompiled/shadow.rmiss --target-env=vulkan1.2 -o spirv/shadow.rmiss.spv