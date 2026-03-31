workspace "Core"
   configurations { "Debug", "Release" }

project "Core"
   kind "StaticLib"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   
   includedirs { "./" }
   includedirs { "./External/"}

   
   files { "./**.cpp", "./**.h", "./**.html" }   
   
   links
    {
    }
	
   filter "configurations:Debug"
      defines { "DEBUG" }
	  staticruntime "on" -- Use /MTd
      symbols "On"
	
   filter "configurations:Release"
      defines { "NDEBUG" }
	  staticruntime "on" -- Use /MTd
      optimize "On"