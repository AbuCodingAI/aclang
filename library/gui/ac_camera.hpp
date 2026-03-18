#pragma once
#include <SDL2/SDL.h>
#include <string>

#ifdef AC_USE_OPENCV
#include <opencv2/opencv.hpp>

// use integratedWebCam — webcam capture via OpenCV
// firstFrame.picture() / latestFrame.picture() — capture a frame
// save as pic.png — save to file
// use pic.png as reference — load reference frame
// IF frame #= pic.png — compare current frame to reference

class ACCamera {
public:
    cv::VideoCapture cap;
    cv::Mat          currentFrame;
    cv::Mat          referenceFrame;
    SDL_Texture*     texture = nullptr;

    bool open(int deviceIndex = 0) {
        return cap.open(deviceIndex);
    }

    // Capture current frame
    bool capture() {
        return cap.read(currentFrame);
    }

    // firstFrame.picture() / latestFrame.picture() + save as <file>
    bool saveFrame(const std::string& path) {
        if (currentFrame.empty()) return false;
        return cv::imwrite(path, currentFrame);
    }

    // use <file> as reference
    bool loadReference(const std::string& path) {
        referenceFrame = cv::imread(path);
        return !referenceFrame.empty();
    }

    // IF frame #= reference — returns true if frames differ (motion detected)
    // Uses mean absolute difference threshold
    bool framesDiffer(double threshold = 25.0) {
        if (currentFrame.empty() || referenceFrame.empty()) return false;
        cv::Mat gray1, gray2, diff;
        cv::cvtColor(currentFrame,  gray1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(referenceFrame, gray2, cv::COLOR_BGR2GRAY);
        cv::absdiff(gray1, gray2, diff);
        return cv::mean(diff)[0] > threshold;
    }

    // Upload current frame to SDL texture for rendering (Background.config mode=livefeed)
    SDL_Texture* getTexture(SDL_Renderer* renderer) {
        if (currentFrame.empty()) return nullptr;
        cv::Mat rgb;
        cv::cvtColor(currentFrame, rgb, cv::COLOR_BGR2RGB);
        if (!texture) {
            texture = SDL_CreateTexture(renderer,
                SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                rgb.cols, rgb.rows);
        }
        SDL_UpdateTexture(texture, nullptr, rgb.data, rgb.cols * 3);
        return texture;
    }

    void destroy() {
        cap.release();
        if (texture) SDL_DestroyTexture(texture);
    }
};

#else

// Stub when OpenCV is not available
class ACCamera {
public:
    bool open(int = 0)                          { return false; }
    bool capture()                              { return false; }
    bool saveFrame(const std::string&)          { return false; }
    bool loadReference(const std::string&)      { return false; }
    bool framesDiffer(double = 25.0)            { return false; }
    SDL_Texture* getTexture(SDL_Renderer*)      { return nullptr; }
    void destroy() {}
};

#endif // AC_USE_OPENCV
