#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(1600, 1000);
    settings.windowMode = OF_WINDOW;
    settings.resizable  = true;
    settings.title      = "ofxTanim – Minimal";

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
