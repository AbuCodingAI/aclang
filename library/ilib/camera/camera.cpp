#include "camera.hpp"

namespace AC {

// Global instances — named to match AC source's receivers directly (camera.init(),
// screen.setmode(), ...). Renamed from the older WebCam/Background.
Camera camera;
Camera latestFrame;
Camera firstFrame;
SidebarConsole sidebar;
Screen screen;

namespace {
struct _WireAux {
    _WireAux() {
        camera.attachAux(&latestFrame, &firstFrame);
        screen.attachCamera(&camera);
    }
} _wireAux;
}

} // namespace AC
