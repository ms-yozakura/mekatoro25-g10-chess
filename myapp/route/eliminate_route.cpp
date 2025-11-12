#include <vector>
#include <string>

#include "../chess/types.hpp"

#define CELLSIZE 25.0 // マスのサイズは25mm

#include "eliminate_route.hpp"

std::string eliminateRoute(std::string rows[8], Move move)
{
    // 自陣側のお外に出す

    std::string text = "";

    bool isWhite = std::isupper(rows[move.from.first][move.from.second]);

    double r = (double)CELLSIZE * move.from.first + CELLSIZE / 2;
    double c = (double)CELLSIZE * move.from.second + CELLSIZE / 2;
    double mr = (double)CELLSIZE * move.from.first;
    double mc = (double)CELLSIZE * move.from.second;
    double nr = isWhite ? (double)-CELLSIZE : (double)CELLSIZE * 8 + CELLSIZE * 3 / 2;
    double nc = (double)CELLSIZE * move.from.second;

    // text+="HOME";
    text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
    text += "UP\n";
    text += "MOVE(" + std::to_string(mr) + "," + std::to_string(mc) + ")\n";
    text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
    text += "DOWN\n";
    text += "AUTOCALIB\n";
    // text += "HOME";

    return text;
}
