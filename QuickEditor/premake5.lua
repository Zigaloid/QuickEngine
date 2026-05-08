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
externalproject "jolt"
   location "../External/JoltPhysics-master/Build/VS2022_CL"  
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
		"USE_ENTRY=1",
		-- Jolt Physics: must match the defines used to compile the Jolt static library
		"JPH_DEBUG_RENDERER",
		"JPH_PROFILE_ENABLED",
		"JPH_OBJECT_STREAM",
		"JPH_FLOATING_POINT_EXCEPTIONS_ENABLED",
		"JPH_USE_AVX2",
		"JPH_USE_AVX",
		"JPH_USE_SSE4_1",
		"JPH_USE_SSE4_2",
		"JPH_USE_LZCNT",
		"JPH_USE_TZCNT",
		"JPH_USE_F16C",
		"JPH_USE_FMADD"
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
	includedirs { "../External/JoltPhysics-master/" }   
	includedirs { "../External/bgfx/"}
	includedirs { "../External"}
	includedirs { "../Shared/ImguiVizualizers" }  
	includedirs { "../Shared" }  
	includedirs { "../AssetClasses" }
	includedirs { "../Shared/ResourceTypes" }  
	includedirs { "../Shared/Components" } 
	includedirs { "../Shared/Physics" } 
	includedirs { "../Shared/Rendering" }
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

	files { "../Shared/ResourceTypes/**.cpp" }
	files { "../Shared/ResourceTypes/**.h" }
	files { "../Shared/Components/**.cpp" }
	files { "../Shared/Components/**.h" }
	files { "../Shared/Physics/**.cpp" }
	files { "../Shared/Physics/**.h" }
	files { "../Shared/Rendering/**.cpp" }
	files { "../Shared/Rendering/**.h" }
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.cpp" }
	files { "../Shared/ImguiVizualizers/ImGuiVisualizerManager.h" }
	files { "../Shared/ImguiVizualizers/IImGuiVisualizer.h" }	
	files { "../Shared/ImguiVizualizers/CommandConsole.cpp" }
	files { "../Shared/ImguiVizualizers/CommandConsole.h" }
	files { "../Shared/ImguiVizualizers/PropertyInspector.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyInspector.h" }
	files { "../Shared/ImguiVizualizers/PropertyInspector_TypeRenderers.cpp" }	
	files { "../Shared/ImguiVizualizers/ImGui3DViewVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/ImGui3DViewVisualizer.h" }
	files { "../Shared/ImguiVizualizers/LevelComponentVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/LevelComponentVisualizer.h" }
		
	files { "../Shared/ImguiVizualizers/CommandHistory.cpp" }
	files { "../Shared/ImguiVizualizers/CommandHistory.h" }
	files { "../Shared/ImguiVizualizers/DeleteEntityCommand.h" }
		
	files { "../Shared/ImguiVizualizers/Selectable.h" }
	files { "../Shared/ImguiVizualizers/RenderComponentSelectable.h" }		
	files { "../Shared/ImguiVizualizers/SelectionManager.cpp" }		
	files { "../Shared/ImguiVizualizers/SelectionManager.h" }		
	files { "../Shared/ImguiVizualizers/TransformCommand.h" }		

		
	files { "../Shared/ImguiVizualizers/PropertyWidgetMap.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMap.h" }
	files { "../Shared/ImguiVizualizers/bgfx*.h" }
	files { "../Shared/ImguiVizualizers/bgfx*.cpp" }
	files { "../Shared/ImguiVizualizers/bgfx*.h" }
	files { "../Shared/ImguiVizualizers/DocumentManager.cpp" }
	files { "../Shared/ImguiVizualizers/DocumentManager.h" }
	files { "../Shared/ImguiVizualizers/ObjJsonEditor.h" }
	files { "../Shared/ImguiVizualizers/CombinedObjJson3DVisualizer.h" }
	files { "../Shared/ImguiVizualizers/MeshComponentVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/MeshComponentVisualizer.h" }
	files { "../Shared/ImguiVizualizers/EntityComponentVisualizer.cpp" }
	files { "../Shared/ImguiVizualizers/EntityComponentVisualizer.h" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapRegistry.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapRegistry.h" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapEditor.cpp" }
	files { "../Shared/ImguiVizualizers/PropertyWidgetMapEditor.h" }
	files { "../Shared/ImguiVizualizers/UnifiedActionManager.cpp" }
	files { "../Shared/ImguiVizualizers/UnifiedActionManager.h" }
	files { "../Shared/ImguiVizualizers/KeyboardShortcutManager.cpp" }
	files { "../Shared/ImguiVizualizers/KeyboardShortcutManager.h" }

	files { "./**.cpp", "./**.h" }      

	filter "configurations:Debug"
	  defines { "DEBUG" }
	  symbols "On"
	  links { "Core" }
	  links { "BGFX" }
	  links { "bimg" }
	  links { "bx" }
	  links { "jolt" }
	  staticruntime "on" -- Use /MT
	  links { "bimg_decode" }
	  
	filter "configurations:Release"
	  defines { "NDEBUG" }
	  optimize "On"	  
	  links { "Core" }	  
	  links { "BGFX" }
	  links { "bimg" }
	  links { "bx" }
	  links { "jolt" }
	  staticruntime "on" -- Use /MTd
	  links { "bimg_decode" }