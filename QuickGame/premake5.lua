workspace "Game"
   configurations { "Debug", "Release" }
   platforms { "x64" }

externalproject "Core"
   location "../Core"   
   kind "StaticLib"
   language "C++"

externalproject "bgfx"
   location "../External/bgfx/.build/projects/vs2019/"  
   kind "StaticLib"
   language "C++"
externalproject "bimg"
   location "../External/bgfx/.build/projects/vs2019/"  
   kind "StaticLib"
   language "C++"   
externalproject "bimg_decode"
   location "../External/bgfx/.build/projects/vs2019/"  
   kind "StaticLib"
   language "C++"
externalproject "bx"
   location "../External/bgfx/.build/projects/vs2019/"  
   kind "StaticLib"
   language "C++"
project "Game"  
   kind "ConsoleApp"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   
    defines {         
        "BX_CONFIG_DEBUG",  
		"ENTRY_CONFIG_IMPLEMENT_MAIN=1"
    }
   
    filter "action:vs*"
        buildoptions {
            "/Zc:preprocessor",
			"/Zc:__cplusplus"
        }
		
	includedirs { "./" }   
	includedirs { "../core/" }      
	includedirs { "../External/bgfx/include/" }   
	includedirs { "../External/bimg/include/" }   
	includedirs { "../External/bx/include/" }   
	includedirs { "../External/bgfx/"}
	includedirs { "../External"}

	includedirs { "./common/" }   

	includedirs { "../External/imgui-docking" }   	
	includedirs { "../External/bx/include/compat/msvc" }   
	includedirs { "../External/OpenGL/Include" }  
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.cpp" }
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.h" }
	files { "../External/imgui-docking/imgui.cpp" }
	files { "../External/imgui-docking/backends/imgui_impl_opengl3.cpp" }
	files { "../External/imgui-docking/imgui_tables.cpp" }
	files { "../External/imgui-docking/imgui_Widgets.cpp" }
	files { "../External/imgui-docking/imgui_draw.cpp" }

	files { "./**.cpp", "./**.h" }      

	filter "configurations:Debug"
	  defines { "DEBUG" }
	  symbols "On"
	  links { "Core" }
	  links { "BGFX" }
	  links { "bimg" }
	  links { "bx" }
	  staticruntime "on" -- Use /MT
	  links { "bimg_decode" }
	  
	filter "configurations:Release"
	  defines { "NDEBUG" }
	  optimize "On"	  
	  links { "Core" }	  
	  links { "BGFX" }
	  links { "bimg" }
	  links { "bx" }
	  staticruntime "on" -- Use /MTd
	  links { "bimg_decode" }