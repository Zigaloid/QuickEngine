workspace "QuickScope"
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

project "QuickScope"  
   kind "ConsoleApp"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   
-- Includes paths.		
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
	
	
--	ImguiVisualizer required.
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.h" }
	files { "../Shared/ImguiVizualizers/IImGuiVisualizer.h" }	
	files { "../Shared/ImguiVizualizers/CommandConsole.cpp" }
	files { "../Shared/ImguiVizualizers/CommandConsole.h" }	
	files { "../Shared/ImguiVizualizers/FPSTracker.cpp" }
	files { "../Shared/ImguiVizualizers/FPSTracker.h" }	
	files { "../Shared/ImguiVizualizers/ImGuiBarGraph.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiBarGraph.h" }
	files { "../Shared/ImguiVizualizers/ImGuiLineGraph.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiLineGraph.h" }
	files { "../Shared/ImguiVizualizers/ProfilerTreeView.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerTreeView.h" }		
	files { "../Shared/ImguiVizualizers/ProfilerVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerVisualizer.h" }	
	files { "../Shared/ImguiVizualizers/ProfilerViewUtils.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerViewUtils.h" }	
	files { "../Shared/ImguiVizualizers/ProfilerSession.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerSession.h" }	
	files { "../Shared/ImguiVizualizers/ProfilerSessionManager.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerSessionManager.h" }	
	files { "../Shared/ImguiVizualizers/ProfilerOutlierView.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerOutlierView.h" }	
	files { "../Shared/ImguiVizualizers/ProfilerFrameComparisonView.cpp" }
	files { "../Shared/ImguiVizualizers/ProfilerFrameComparisonView.h" }	
	
	files { "../Shared/ImguiVizualizers/ImGuiButterflyChart.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiButterflyChart.h" }	


	files { "../Shared/ImguiVizualizers/UnifiedActionManager.cpp" }
	files { "../Shared/ImguiVizualizers/UnifiedActionManager.h" }	

	files { "../Shared/KeyboardShortcutManager.cpp" }
	files { "../Shared/KeyboardShortcutManager.h" }	
	
-- Main app files.
	files { "./**.cpp", "./**.h" }      

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

	filter "configurations:Debug"
	  defines { "DEBUG" }
	  symbols "On"
	  links { "Core" }
	  links { "bgfx" }
	  links { "bimg" }
	  links { "bx" }	  
	  links { "bimg_decode" }
	  staticruntime "on" -- Use /MT
	  
	filter "configurations:Release"
	  defines { "NDEBUG" }
	  optimize "On"	  
	  links { "Core" }	  
	  links { "bgfx" }
	  links { "bimg" }
	  links { "bx" }	  
	  links { "bimg_decode" }
	  staticruntime "on" -- Use /MTd