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
		"ENTRY_CONFIG_IMPLEMENT_MAIN=1",
		"USE_ENTRY=1"
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
	includedirs { "../Shared/ImguiVizualizers" }  
	includedirs { "../Shared" }  
	includedirs { "../AssetClasses" }
	includedirs { "../Shared/ResourceTypes" }  
	includedirs { "../Shared/Components" } 
	includedirs { "../Shared/bgfx_common" }   
	includedirs { "../Shared/utils/" }

	includedirs { "../External/imgui-docking" }   	
	includedirs { "../External/bx/include/compat/msvc" }   
	includedirs { "../External/OpenGL/Include" }  

	includedirs { "../External/ufbx" }   	
	
	files { "../AssetClasses/**.cpp" }
	files { "../AssetClasses/**.h" }
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.cpp" }
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.h" }
	files { "../External/imgui-docking/imgui.cpp" }
	files { "../External/imgui-docking/backends/imgui_impl_opengl3.cpp" }
	files { "../External/imgui-docking/imgui_tables.cpp" }
	files { "../External/imgui-docking/imgui_Widgets.cpp" }
	files { "../External/imgui-docking/imgui_draw.cpp" }
	
	files { "../External/ufbx/ufbx.c" }
	files { "../External/ufbx/ufbx.h" }

	files { "../Shared/utils/**.cpp" }
	files { "../Shared/utils/**.h" }
	files { "../Shared/bgfx_common/**.cpp" }
	files { "../Shared/bgfx_common/**.h" }
	
	files { "../External/bgfx_common/**.cpp" }   
	files { "../External/bgfx_common/**.h" }   
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