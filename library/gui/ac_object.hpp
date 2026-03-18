#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "ac_sprite.hpp"
#include "ac_error.hpp"

enum class ObjShape { NONE, SQUARE, RECTANGLE };

struct Hitbox { int x, y, w, h; };

class ACObject {
public:
    std::string name;
    ObjShape    shape     = ObjShape::NONE;
    int         x = 0,  y = 0;
    int         w = 50, h = 50;
    SDL_Color   color     = {200, 200, 200, 255};
    ACSprite    sprite;
    bool        visible   = true;

    // Obj.Character / Obj.Cactus etc.
    explicit ACObject(const std::string& n) : name(n) {}

    // Object.config item=square(50)
    void configSquare(int size) {
        shape = ObjShape::SQUARE; w = size; h = size;
    }

    // Object.config item=rectangle(WxH)
    void configRect(int rw, int rh) {
        shape = ObjShape::RECTANGLE; w = rw; h = rh;
    }

    // Object.pixel.up(n) — move up n pixels
    void pixelUp(int n)    { y -= n; }
    void pixelDown(int n)  { y += n; }
    void pixelLeft(int n)  { x -= n; }
    void pixelRight(int n) { x += n; }

    // Object.vertex(x, y) — set trajectory peak position
    void setVertex(int vx, int vy) { vertexX = vx; vertexY = vy; }

    // Hitbox coords
    Hitbox hitbox() const { return {x, y, w, h}; }

    // IF Object.Hitbox.Coords Overlap other
    bool overlaps(const ACObject& other) const {
        Hitbox a = hitbox(), b = other.hitbox();
        return a.x < b.x + b.w && a.x + a.w > b.x &&
               a.y < b.y + b.h && a.y + a.h > b.y;
    }

    // regen.Object.OGpos — reset to original spawn position
    void regenOGPos() { x = ogX; y = ogY; }
    void setOGPos(int ox, int oy) { ogX = ox; ogY = oy; x = ox; y = oy; }

    void draw(SDL_Renderer* renderer) {
        if (!visible) return;
        if (sprite.loaded) {
            sprite.draw(renderer, x, y, w, h);
        } else {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect r = {x, y, w, h};
            SDL_RenderFillRect(renderer, &r);
        }
    }

    int vertexX = 0, vertexY = 0;
private:
    int ogX = 0, ogY = 0;
};
