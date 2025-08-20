// ILDAReader.h
#ifndef ILDAREADER_H
#define ILDAREADER_H

#include <string>
#include <vector>
#include "../sdk/SDKSocket.h" // For frame_buffer

class ILDAReader {
public:
    static bool load(const std::string& filename, frame_buffer& frame);
};

#endif // ILDAREADER_H
