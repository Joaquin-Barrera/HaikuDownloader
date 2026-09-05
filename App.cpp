#include "App.h"
#include "MainWindow.h"


const char* kSignature = "application/x-vnd.Joaquin-Barrera-HaikuDownloader";

App::App() : BApplication(kSignature) {
    MainWindow* window = new MainWindow();
    window->Show();
}