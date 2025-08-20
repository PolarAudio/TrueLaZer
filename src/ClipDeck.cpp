// ClipDeck.cpp
#include "ClipDeck.h"
#include <iostream>

ClipDeck::ClipDeck(int rows, int cols) : rows(rows), cols(cols) {
    std::cout << "Creating clip deck with " << rows << " rows and " << cols << " columns." << std::endl;
    clips.resize(rows, std::vector<Clip*>(cols, nullptr));
}

void ClipDeck::setClip(int row, int col, Clip* clip) {
    std::cout << "Setting clip at row " << row << ", col " << col << std::endl;
    if (row >= 0 && row < rows && col >= 0 && col < cols) {
        clips[row][col] = clip;
    }
}

Clip* ClipDeck::getClip(int row, int col) {
    if (row >= 0 && row < rows && col >= 0 && col < cols) {
        return clips[row][col];
    }
    return nullptr;
}

void ClipDeck::clear() {
    std::cout << "Clearing clip deck." << std::endl;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (clips[i][j]) {
                delete clips[i][j];
                clips[i][j] = nullptr;
            }
        }
    }
}

int ClipDeck::getRows() const {
    return rows;
}

int ClipDeck::getCols() const {
    return cols;
}