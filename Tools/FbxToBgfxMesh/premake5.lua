local engineRootPath = "../../"

workspace "FbxToBgfxMesh"
   configurations { "Debug", "Release" }
   platforms { "x64" }

externalproject "Core"
   location(path.join(engineRootPath, "Core"))
   kind "StaticLib"
   language "C++"

externalproject "bgfx"
   location(path.join(engineRootPath, "External/bgfx/.build/projects/vs2019/"))
   kind "StaticLib"
   language "C++"
externalproject "bimg"
   location(path.join(engineRootPath, "External/bgfx/.build/projects/vs2019/"))
   kind "StaticLib"
   language "C++"   
externalproject "bimg_decode"
   location(path.join(engineRootPath, "External/bgfx/.build/projects/vs2019/"))  
   kind "StaticLib"
   language "C++"
externalproject "bx"
   location(path.join(engineRootPath, "External/bgfx/.build/projects/vs2019/"))  
   kind "StaticLib"
   language "C++"
project "FbxToBgfxMesh"  
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
	includedirs { path.join(engineRootPath, "core/") }      
	includedirs { path.join(engineRootPath, "External/bgfx/include/") }   
	includedirs { path.join(engineRootPath, "External/bimg/include/") }   
	includedirs { path.join(engineRootPath, "External/bx/include/") }   
	includedirs { path.join(engineRootPath, "External/bgfx/")}
	includedirs { path.join(engineRootPath, "External")}
	includedirs { path.join(engineRootPath, "Shared/ImguiVizualizers") }  
	includedirs { path.join(engineRootPath, "Shared/bgfx_common") }   
	includedirs { path.join(engineRootPath, "Shared/utils/") }

	includedirs { path.join(engineRootPath, "External/imgui-docking") }   	
	includedirs { path.join(engineRootPath, "External/bx/include/compat/msvc") }   
	includedirs { path.join(engineRootPath, "External/OpenGL/Include") }  
	includedirs { path.join(engineRootPath, "External/ufbx") }   	
	
	files { path.join(engineRootPath, "AssetClasses/**.cpp") }
	files { path.join(engineRootPath, "AssetClasses/**.h") }
	files { path.join(engineRootPath, "External/bgfx/3rdparty/meshoptimizer/src/**.cpp") }
	files { path.join(engineRootPath, "External/bgfx/3rdparty/meshoptimizer/src/**.h") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/backends/imgui_impl_opengl3.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_tables.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_Widgets.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_draw.cpp") }
	
	files { path.join(engineRootPath, "External/ufbx/ufbx.c") }
	files { path.join(engineRootPath, "External/ufbx/ufbx.h") }

	files { path.join(engineRootPath, "Shared/utils/**.cpp") }
	files { path.join(engineRootPath, "Shared/utils/**.h") }
	files { path.join(engineRootPath, "Shared/bgfx_common/**.cpp") }
	files { path.join(engineRootPath, "Shared/bgfx_common/**.h") }
	
	files { path.join(engineRootPath, "External/bgfx/3rdparty/meshoptimizer/src/**.cpp") }
	files { path.join(engineRootPath, "External/bgfx/3rdparty/meshoptimizer/src/**.h") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/backends/imgui_impl_opengl3.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_tables.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_Widgets.cpp") }
	files { path.join(engineRootPath, "External/imgui-docking/imgui_draw.cpp") }
	files { path.join(engineRootPath, "Shared/ImguiVizualizers/ImguiVisualizerManager.cpp" ) }
	files { path.join(engineRootPath, "Shared/ImguiVizualizers/ImguiVisualizerManager.h" ) }
	files { path.join(engineRootPath, "Shared/ImguiVizualizers/CommandConsole.cpp" ) }
	files { path.join(engineRootPath, "Shared/ImguiVizualizers/CommandConsole.h" ) }
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