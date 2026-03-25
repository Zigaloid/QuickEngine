workspace "ExampleApp"
   configurations { "Debug", "Release" }
   platforms { "x64" }

externalproject "Core"
   location "../../Core"
   uuid "2EB4837C-1AEB-840D-C3D7-6A10AFED000F"
   kind "StaticLib"
   language "C++"
   
externalproject "Rendering"
   location "../../Rendering"   
   kind "StaticLib"
   language "C++"
   
project "ExampleApp"  
   kind "ConsoleApp"
   language "C"
   targetdir "bin/%{cfg.buildcfg}"
   cppdialect "C++20"
   architecture "x86_64"
   includedirs { "./" }   
   includedirs { "../../core/" }   
   includedirs { "../../rendering/" }   
   
   includedirs { "../../External/imgui-docking/" }
   includedirs { "../../External/OpenGL/Include" }   
   files { "./**.cpp", "./**.h" }      
   files { "../../External/imgui-docking/imgui.cpp" }
   files { "../../External/imgui-docking/backends/imgui_impl_glfw.cpp" }
   files { "../../External/imgui-docking/backends/imgui_impl_opengl3.cpp" }
   files { "../../External/imgui-docking/imgui_tables.cpp" }
   files { "../../External/imgui-docking/imgui_Widgets.cpp" }
   files { "../../External/imgui-docking/imgui_draw.cpp" }

   links { "../../External/OpenGL/libs/glfw/glfw3.lib" }   
   links { "../../External/OpenGL/libs/gl/glew32.lib" }   
   links { "opengl32.lib" }
   links { "../../External/OpenGL/Libs/Assimp/assimp-vc142-mtd.lib" }

   filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"
	  links { "Core" }
	  links { "Rendering" }
   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"	  
	  links { "Core" }
	  links { "Rendering" }