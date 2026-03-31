workspace "Rendering"   configurations { "Debug", "Release" }
   platforms { "x64" }
   
externalproject "Core"
   location "../Core"
   uuid "2EB4837C-1AEB-840D-C3D7-6A10AFED000F"
   kind "StaticLib"
   language "C++"

project "Rendering"
   kind "StaticLib"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   
   includedirs { "./" }
   includedirs { "../core/" }   
   includedirs { "../External/OpenGL/Include" } 
   
   files { "./**.cpp", "./**.h", "./**.html" }      
   links { "../External/OpenGL/libs/glfw/glfw3.lib" }   
   links { "../External/OpenGL/libs/gl/glew32.lib" }   
   
   filter "system:windows"
    links { "legacy_stdio_definitions.lib" }
   
   filter "configurations:Debug"
      staticruntime "on" -- Use /MTd
      defines { "DEBUG" }
      symbols "On"
	
   filter "configurations:Release"
      staticruntime "on" -- Use /MTd
      defines { "NDEBUG" }
      optimize "On"