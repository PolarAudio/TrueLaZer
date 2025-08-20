// Clip.cpp
#include "Clip.h"
#include "ILDAReader.h"
#include <iostream>

Clip::Clip(const std::string& name, const std::string& path) : name(name), filePath(path) {
    std::cout << "Creating clip: " << name << std::endl;
    if (!filePath.empty()) {
        ILDAReader::load(filePath, frame);
    }
}

const std::string& Clip::getName() const {
    return name;
}

const frame_buffer& Clip::getFrame() const {
    return frame;
}