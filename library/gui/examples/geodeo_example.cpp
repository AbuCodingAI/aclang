// GeoDeo — AC Side-Scroller
// Mirrors the GeoDeo program from desc.datac
// AC->BNY | <gui> <OBJECT> Character+Cactus <SCREEN> green <LOGIC> jump+terrain

#include "../ac_gui.hpp"

int main() {
    ACScreen        screen;
    ACEventListener events;
    ACPhysics       physics;

    // <SCREEN> Screen.OBJECT resize 1720x1080, Background.config color=green
    screen.init(1720, 1080, "GeoDeo");
    screen.setColor(34, 139, 34); // green

    // <OBJECT> Obj.Character, Character.config item=square(50)
    ACObject character("Character");
    character.configSquare(50);
    character.setOGPos(200, screen.height - 150);

    // Character.Sprite = AC.Search($CharacterSprite.png$)
    std::string charSpritePath = ac_search("CharacterSprite.png");
    if (!charSpritePath.empty())
        character.sprite.load(screen.renderer, charSpritePath);
    // IF Sprite.png not found — use config definition (square shape fallback, already set)

    // <OBJECT> Obj.Cactus, Cactus.config item=rectangle(100x300)
    ACObject cactus("Cactus");
    cactus.configRect(100, 300);
    cactus.setOGPos(screen.width + 100, screen.height - 350);

    // Cactus.Sprite = AC.Search($CactusSprite.png$)
    std::string cactusSpritePath = ac_search("CactusSprite.png");
    if (!cactusSpritePath.empty())
        cactus.sprite.load(screen.renderer, cactusSpritePath);

    ACPhysics cactusPhysics;

    bool running = true;
    bool jumpQueued = false;

    // configure event-listener: on value=space -> jump(Character)
    events.onSpace([&]() { jumpQueued = true; });
    events.onEscape([&]() { running = false; });

    // <StartHere> rerun loop
    while (running) {
        if (!events.pollAll()) break;

        // jump(Character) — IF arg not of Obj type: raise ERR
        if (jumpQueued) {
            jumpQueued = false;
            // arg.pixel.up(180), arg.simulate.airtime(60), arg.vertex(400,600), arg.curveshape(-x^2)
            physics.startJump(character, 60.0f, 180);
        }

        // Update jump arc (negative parabola curveshape)
        physics.updateJump(character);

        // <terrain> tag — Terrain.ANIMATE, RightDir at 60fps
        // Cactus.CircleFall(1/4 RightDir) on hitbox overlap
        if (!cactusPhysics.circleFall.active) {
            cactusPhysics.animateTerrain(cactus, screen.width, 6);
        }

        // IF Character.Hitbox.Coords Overlap Cactus
        if (character.overlaps(cactus) && !cactusPhysics.circleFall.active) {
            cactusPhysics.startCircleFall(1.0f / 4.0f, true); // 1/4 RightDir
        }

        cactusPhysics.updateCircleFall();

        // IF Cactus.CircleFell(1/4 RightDir) is True -> regen.Cactus.OGpos
        if (cactusPhysics.circleFell()) {
            cactus.regenOGPos();
            cactusPhysics.circleFall.fell = false;
        }

        // when many hitbox overlap — temp.jump.vertex /= 2 (handled inside startJump)

        screen.clear();
        character.draw(screen.renderer);
        cactus.draw(screen.renderer);
        screen.present();
        SDL_Delay(16); // 60fps
    }

    character.sprite.destroy();
    cactus.sprite.destroy();
    screen.destroy();
    return 0;
}
