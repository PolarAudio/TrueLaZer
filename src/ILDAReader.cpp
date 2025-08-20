// ILDAReader.cpp
#include "ILDAReader.h"
#include <fstream>
#include <iostream>

bool ILDAReader::load(const std::string& filename, frame_buffer& frame) {
    std::cout << "Parsing ILDA file: " << filename << std::endl;
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    // For simplicity, we'll assume a very basic ILDA format 4 file
    // with one frame and 3D coordinates with color.
    // A proper implementation would need to handle all format types and sections.

    char header[32];
    file.read(header, 32);

    if (std::string(header, 4) != "ILDA") {
        std::cerr << "Error: Not a valid ILDA file." << std::endl;
        return false;
    }

    // We are looking for format 4 or 5 (3D with color)
    if (header[7] != 4 && header[7] != 5) {
        std::cerr << "Error: Only ILDA format 4 and 5 are supported for now." << std::endl;
        return false;
    }

    int num_points = (header[24] << 8) | header[25];
    frame.count = num_points;
    std::cout << "Number of points: " << num_points << std::endl;

    for (int i = 0; i < num_points; ++i) {
        char point_data[8];
        file.read(point_data, 8);

        // ILDA coordinates are 16-bit signed integers
        int16_t x = (point_data[0] << 8) | point_data[1];
        int16_t y = (point_data[2] << 8) | point_data[3];
        // z is ignored for now

        frame.points[i].x = x / 32767.0f; // Normalize to -1.0 to 1.0
        frame.points[i].y = y / 32767.0f;

        // Status byte
        frame.points[i].blanking = (point_data[6] & 0x40) ? 0 : 1; // Bit 6 is blanking

        // Color data
        frame.points[i].r = point_data[7];
        frame.points[i].g = 0; // ILDA format 4 only has one color per point
        frame.points[i].b = 0;
        
        // A more complete parser would handle palettes.
        // For now, we just use the color index as the red component.
    }

    std::cout << "Finished parsing ILDA file." << std::endl;
    return true;
}