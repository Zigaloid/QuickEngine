
#include "ImGuiApp/App.h"

#include "Input/WindowsGamepad.h"
#include "CoreSystem/CoreSystem.h"
#include "Profiler/Profiler.h"
#include "Input/GamepadManager.h"
#include "app/ApplicationComponent.h"
#include "ProfilerApp.h"
#include "external/rapidjson/include/prettywriter.h"
#include "external/rapidjson/include/stringbuffer.h"
#include "external/rapidjson/include/Document.h"

ProfilerApp g_theApp;

int main()
{

    
    
    if (!g_theApp.Initialize()) {
        printf("Failed to initialize application\n");
        return -1;
    }
    
    g_theApp.Run();
    g_theApp.Shutdown();
    
    return 0;
}