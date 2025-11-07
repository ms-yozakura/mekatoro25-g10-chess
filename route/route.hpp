

/*
    ここでは盤面情報とコマの移動の情報からArduinoへの指令コードを生成することを目指す
    <指令コード例>
    HOME
    WARP(x,y)
    UP
    MOVE(xx,yy,[f])
    DOWN
*/

#include <string>

#include "knight_route.hpp"
#include "king_route.hpp"
#include "eliminate_route.hpp"

#define CELLSIZE 25.0 // マスのサイズは25mm

#define TOMB {-50.0, 50.0}

std::string command(std::string[8], Move);
std::string normalRoute(std::string[8], Move);