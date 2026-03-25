workspace "Core"
   configurations { "Debug", "Release" }

project "Core"
   kind "StaticLib"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   
   includedirs { "./" }
   
   
   files { "./**.cpp", "./**.h", "./**.html" }   
   
   links
    {
    }
	
   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
	
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"