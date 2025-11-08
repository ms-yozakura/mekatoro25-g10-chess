#include <vector>
#include <string>

#include "../chess/types.hpp"

#define CELLSIZE 25.0 // マスのサイズは25mm

#include "eliminate_route.hpp"

std::string eliminateRoute(std::string rows[8], Move move)
{
    //自陣側のお外に出す

    std::string text = "";

    bool isWhite = std::isupper(rows[move.first.first][move.first.second]);

    double r = (double)CELLSIZE * move.first.first + CELLSIZE / 2;
    double c = (double)CELLSIZE * move.first.second + CELLSIZE / 2;
    double mr = (double)CELLSIZE * move.first.first;
    double mc = (double)CELLSIZE * move.first.second;
    double nr = isWhite ? (double)-CELLSIZE / 2 : (double)CELLSIZE * 8 - CELLSIZE / 2;
    double nc = (double)CELLSIZE * move.second.second;

    // text+="HOME";
    text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
    text += "UP\n";
    text += "MOVE(" + std::to_string(mr) + "," + std::to_string(mc) + ")\n";
    text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
    text += "DOWN\n";
    text += "AUTOCALIB\n";

    return "// auto elimination is not implemented";
}
