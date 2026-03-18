// Motion — AC Motion Tracker
// Mirrors the Motion program from desc.datac
// AC->BNY | use integratedWebCam | <gui> <OBJECT> sidebar <SCREEN> livefeed <LOGIC>

#include "../ac_gui.hpp"

int main() {
    ACScreen   screen;
    ACConsole  sidebar;
    ACCamera   camera;
    ACEventListener events;

    // <SCREEN> Screen.OBJECT resize 1280x720, Background.config mode=livefeed
    screen.init(1280, 720, "AC Motion Tracker");
    screen.setLivefeed();

    // <OBJECT> Obj.sidebar, sidebar.config item=console, sidebar.region=left, sidebar.interactive=true
    sidebar.init(screen.renderer, screen.width, screen.height, ACConsole::Region::LEFT);

    // use integratedWebCam
    if (!camera.open(0)) {
        sidebar.display("No webcam found.");
    }

    // firstFrame.picture() + save as pic.png
    camera.capture();
    camera.saveFrame("pic.png");
    camera.loadReference("pic.png");

    bool running = true;
    events.onEscape([&]() { running = false; });

    // <StartHere> rerun loop
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { running = false; break; }
            sidebar.handleEvent(e);
            events.process(e);
        }

        camera.capture();

        screen.clear();

        // Background.config mode=livefeed — draw webcam feed
        SDL_Texture* feed = camera.getTexture(screen.renderer);
        if (feed) {
            SDL_Rect dst = {sidebar.w, 0, screen.width - sidebar.w, screen.height};
            SDL_RenderCopy(screen.renderer, feed, nullptr, &dst);
        }

        // IF frame #= pic.png (motion detected)
        if (camera.framesDiffer()) {
            // Create console object as sidebar
            sidebar.display("Movement Detected");
            sidebar.display("Type 'reset' to update reference frame.");

            // latestFrame.picture() + save as pic.png
            camera.saveFrame("pic.png");
            camera.loadReference("pic.png");
        }

        // sidebar.ask($Command?$) — non-blocking: show prompt if not already waiting
        if (!sidebar.waitingForInput) {
            sidebar.ask("Command?");
        }

        // sidebar.getinput — check once Enter is pressed
        if (!sidebar.waitingForInput) {
            std::string usercmd = sidebar.getInput();

            // IF usercmd = $reset$
            if (usercmd == "reset") {
                sidebar.display("New reference frame saved.");
                camera.saveFrame("pic.png");
                camera.loadReference("pic.png");
            }

            // IF usercmd = $exit$
            if (usercmd == "exit") {
                sidebar.display("Stopping AC Motion Tracker...");
                running = false; // /kill
            }
        }

        sidebar.draw(screen.renderer);
        screen.present();
        SDL_Delay(16); // ~60fps
    }

    camera.destroy();
    sidebar.destroy();
    screen.destroy();
    return 0;
}
