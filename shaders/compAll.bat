@echo off
cd uncompiled
for %%i in (*) do %VK_SDK_PATH%\Bin\glslc.exe %%i -o ../spirv/%%i.spv --target-env=vulkan1.4
