// ClipDeck.h
#ifndef CLIPDECK_H
#define CLIPDECK_H

#include <vector>
#include "Clip.h"

class ClipDeck {
public:
    ClipDeck(int rows, int cols);
    void setClip(int row, int col, Clip* clip);
    Clip* getClip(int row, int col);
    void clear();
    int getRows() const;
    int getCols() const;

private:
    int rows;
    int cols;
    std::vector<std::vector<Clip*>> clips;
};

#endif // CLIPDECK_H
