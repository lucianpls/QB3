// SiBi.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "bmap.h"
#include <cmath>

int main()
{
    int sx = 200, sy = 299;
    BMap bm(sx, sy);
    //std::cout << bm.bit(7, 8);
    //bm.clear(7, 8);
    //std::cout << bm.bit(7, 8);
    ////bm.set(7, 8);
    ////std::cout << bm.bit(7, 8);
    //std::cout << std::endl;

    // a rectangle
    for (int y = 30; y < 60; y++)
        for (int x = 123; x < 130; x++)
            bm.clear(x, y);

    // circles
    int cx = 150, cy = 76, cr = 34;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    cx = 35; cy = 212; cr = 78;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    cx = 79; cy = 235; cr = 135;
    for (int y = 0; y < sy; y++)
        for (int x = 0; x < sx; x++)
            if (((x - cx) * (x - cx) + (y - cy) * (y - cy)) < cr * cr)
                bm.clear(x, y);

    BitstreamOut s;
    std::cout << std::endl << bm.dsize() * 8 << std::endl;
    bm.pack(s);
    bm.dump(std::string("file.raw"));
    std::cout << s.v.size() * 8 << std::endl;
    RLE(s.v, s.v);
    std::cout << s.v.size() * 8 << std::endl;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
