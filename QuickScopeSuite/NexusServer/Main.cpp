
#include "ImGuiApp/App.h"

#include "Input/WindowsGamepad.h"
#include "CoreSystem/CoreSystem.h"
#include "Input/GamepadManager.h"
#include "app/ApplicationComponent.h"
#include "NexusServerApp.h"

int main()
{

    CNexusServerApp app;
    
    if (!app.Initialize()) {
        printf("Failed to initialize application\n");
        return -1;
    }
    
    app.Run();
    app.Shutdown();
    
    return 0;
}