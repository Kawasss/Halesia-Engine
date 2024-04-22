@echo off
cd uncompiled
for %%i in (*) do C:/VulkanSDK/1.3.261.1/Bin/glslc.exe %%i -o ../spirv/%%i.spv --target-env=vulkan1.3