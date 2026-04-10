workspace "QuickEdit"
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
project "QuickEdit"  
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

	includedirs { "../External/bgfx_common" }   
	includedirs { "../External/imgui-docking" }   	
	includedirs { "../External/bx/include/compat/msvc" }   
	includedirs { "../External/OpenGL/Include" }  
	
	files { "../AssetClasses/**.cpp" }
	files { "../AssetClasses/**.h" }
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.cpp" }
	files { "../External/bgfx/3rdparty/meshoptimizer/src/**.h" }
	files { "../External/bgfx_common/**.cpp" }   
	files { "../External/bgfx_common/**.h" }   
	files { "../External/imgui-docking/imgui.cpp" }
	files { "../External/imgui-docking/backends/imgui_impl_opengl3.cpp" }
	files { "../External/imgui-docking/imgui_tables.cpp" }
	files { "../External/imgui-docking/imgui_Widgets.cpp" }
	files { "../External/imgui-docking/imgui_draw.cpp" }
	
	files { "../Shared/ResourceTypes/**.cpp" }
	files { "../Shared/ResourceTypes/**.h" }
	files { "../Shared/Components/**.cpp" }
	files { "../Shared/Components/**.h" }
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.h" }
	files { "../Shared/ImguiVizualizers/IImGuiVisualizer.h" }	
	files { "../Shared/ImguiVizualizers/CommandConsole.cpp" }
	files { "../Shared/ImguiVizualizers/CommandConsole.h" }
	files { "../Shared/ImguiVizualizers/PropertyInspector.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyInspector.h" }
	files { "../Shared/ImguiVizualizers/ImGui3DViewVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/ImGui3DViewVisualizer.h" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMap.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMap.h" }
	files { "../Shared/ImguiVizualizers/bgfx*.h" }
	files { "../Shared/ImguiVizualizers/bgfx*.cpp" }
	files { "../Shared/ImguiVizualizers/bgfx*.h" }
	files { "../Shared/ImguiVizualizers/DocumentManager.cpp" }
	files { "../Shared/ImguiVizualizers/DocumentManager.h" }
	files { "../Shared/ImguiVizualizers/ObjJsonEditor.h" }
	files { "../Shared/ImguiVizualizers/CombinedObjJson3DVisualizer.h" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapRegistry.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapRegistry.h" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapEditor.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapEditor.h" }
	files { "../Shared/ImguiVizualizers/UnifiedActionManager.cpp" }
	files { "../Shared/ImguiVizualizers/UnifiedActionManager.h" }
	files { "../Shared/KeyboardShortcutManager.cpp" }
	files { "../Shared/KeyboardShortcutManager.h" }

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