

/*
    ここでは盤面情報とコマの移動の情報からArduinoへの指令コードを生成することを目指す
    <指令コード例>
    HOME
    WARP(x,y)
    UP
    MOVE(xx,yy,[f])
    DOWN
*/

#include <vector>
#include <string>

#include "route.hpp"

// 移動を表す型エイリアス{{startR, startC},{endR,endC}}: 0~7
// using Move = std::pair<std::pair<int,int>, std::pair<int,int>>;

std::string command(std::string rows[8], Move move)
{

    std::string text = "";

    // コマの退場は先にやる
    char end_type = rows[move.to.first][move.to.second];
    if (end_type != '*')
    {
        text += eliminateRoute(rows, {{move.to.first, move.to.second}, TOMB}); // TOMBは仮
    }

    // 動かすコマの種類を判定
    char type = rows[move.from.first][move.from.second];
    bool isWhite = std::isupper(type); // 白か黒か
    type = toupper(type);              // コマの種類（大文字RNBKQP）

    text += std::string("// to carry ") + (isWhite ? "white " : "black ") + type + "\n";

    // ナイト: 間を縫う動き
    if (type == 'N')
        text += knightRoute(rows, move);
    // キング: キャスリングがある
    else if (type == 'K')
        text += kingRoute(rows, move);
    // 普通の動き
    else
        text += normalRoute(/*rows, */ move);

    return text;
}

std::string normalRoute(/*std::string rows[8], */ Move move)
{

    std::string text = "";

    double r = (double)CELLSIZE * move.from.first + CELLSIZE / 2;
    double c = (double)CELLSIZE * move.from.second + CELLSIZE / 2;
    double nr = (double)CELLSIZE * move.to.first + CELLSIZE / 2;
    double nc = (double)CELLSIZE * move.to.second + CELLSIZE / 2;

    // text+="HOME";
    text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
    text += "UP\n";
    text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
    text += "DOWN\n";
    text += "AUTOCALIB\n";
    // text += "HOME";

    return text;
}
