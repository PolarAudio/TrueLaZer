// Clip.h
#ifndef CLIP_H
#define CLIP_H

#include <string>
#include "../sdk/SDKSocket.h"

class Clip {
public:
    Clip(const std::string& name, const std::string& path = "");
    const std::string& getName() const;
    const frame_buffer& getFrame() const;

private:
    std::string name;
    std::string filePath;
    frame_buffer frame;
};

#endif // CLIP_H
