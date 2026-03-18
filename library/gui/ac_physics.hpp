#pragma once
#include "ac_object.hpp"
#include <cmath>
#include <functional>

// Handles jump(), airtime, curveshape, CircleFall, terrain animation

class ACPhysics {
public:
    // jump(arg) — arg.pixel.up(180), airtime(60), vertex, curveshape(-x^2)
    // airtime in deciseconds (60 = 6 seconds), vertex is peak coords
    // curveshape: -x^2 = negative parabola (goes up then comes down)
    struct JumpState {
        bool   active    = false;
        float  t         = 0.0f;   // time elapsed (frames)
        float  airtime   = 60.0f;  // total airtime in frames (60 deciseconds)
        int    startY    = 0;
        int    peakY     = 0;      // vertex height (negative = up)
        float  vertexT   = 0.5f;   // fraction of airtime at peak (0.5 = midpoint)
    };

    JumpState jumpState;

    // jump(obj) — initiates a jump on the object
    // vertex(vx, vy) and curveshape(-x^2) are set before calling
    void startJump(ACObject& obj, float airtime = 60.0f, int peakOffset = 180) {
        if (jumpState.active) {
            // temp.jump.vertex /= 2 (local override for multi-hitbox)
            peakOffset /= 2;
        }
        jumpState.active  = true;
        jumpState.t       = 0.0f;
        jumpState.airtime = airtime;
        jumpState.startY  = obj.y;
        jumpState.peakY   = obj.y - peakOffset; // up = negative y
    }

    // Call each frame — updates object Y along parabolic arc
    // Returns true while jump is active
    bool updateJump(ACObject& obj) {
        if (!jumpState.active) return false;
        float progress = jumpState.t / jumpState.airtime; // 0.0 -> 1.0
        // Negative parabola: y = -4 * peakOffset * t * (t - 1)
        float peakOffset = (float)(jumpState.startY - jumpState.peakY);
        float dy = -4.0f * peakOffset * progress * (progress - 1.0f);
        obj.y = jumpState.startY - (int)dy;
        jumpState.t += 1.0f;
        if (jumpState.t >= jumpState.airtime) {
            obj.y = jumpState.startY;
            jumpState.active = false;
        }
        return jumpState.active;
    }

    // CircleFall(1/4 RightDir) — rotate object 90 degrees to the right
    // Returns true when animation completes
    struct CircleFallState {
        bool  active   = false;
        float angle    = 0.0f;  // current rotation degrees
        float target   = 90.0f; // 1/4 circle = 90 degrees
        float speed    = 3.0f;  // degrees per frame
        bool  fell     = false;
    };

    CircleFallState circleFall;

    void startCircleFall(float quarterCircles = 1.0f, bool rightDir = true) {
        circleFall.active = true;
        circleFall.angle  = 0.0f;
        circleFall.target = 90.0f * quarterCircles;
        circleFall.speed  = rightDir ? 3.0f : -3.0f;
        circleFall.fell   = false;
    }

    // Returns true while falling, sets circleFall.fell = true when done
    bool updateCircleFall() {
        if (!circleFall.active) return false;
        circleFall.angle += circleFall.speed;
        if (std::abs(circleFall.angle) >= circleFall.target) {
            circleFall.active = false;
            circleFall.fell   = true;
        }
        return circleFall.active;
    }

    bool circleFell() const { return circleFall.fell; }

    // Terrain.ANIMATE — moves terrain left at given fps
    // Call each frame with the object to scroll
    void animateTerrain(ACObject& obj, int screenWidth, int speed = 4) {
        obj.x -= speed;
        if (obj.x + obj.w < 0)
            obj.x = screenWidth; // wrap around
    }
};
