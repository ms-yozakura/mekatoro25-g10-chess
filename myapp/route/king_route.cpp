
#include "king_route.hpp"

std::string kingRoute(std::string rows[8], Move move)
{
    std::string text = "";

    bool castling = std::abs(move.to.second - move.from.second) == 2;
    bool isWhite = std::isupper(rows[move.from.first][move.from.second]);

    double r = (double)CELLSIZE * move.from.first + CELLSIZE / 2;
    double c = (double)CELLSIZE * move.from.second + CELLSIZE / 2;
    double nr = (double)CELLSIZE * move.to.first + CELLSIZE / 2;
    double nc = (double)CELLSIZE * move.to.second + CELLSIZE / 2;

    if (castling)
    {
        text += std::string("// considering Castling movement\n");
        bool isKingSide = (move.to.second - move.from.second) > 0;
        double rookr = (double)(isWhite ? CELLSIZE * 7 + CELLSIZE / 2 : CELLSIZE / 2);
        double rookc = (double)CELLSIZE * (double)(isKingSide ? 7 : 0) + CELLSIZE / 2;
        double rooknr = (double)(isWhite ? CELLSIZE * 7 + CELLSIZE / 2 : CELLSIZE / 2);
        double rooknc = (double)CELLSIZE * (double)(isKingSide ? 5 : 3) + CELLSIZE / 2;
        // text+="HOME";
        // Rookをうごかす
        text += std::string("// carry Rook\n");
        text += "WARP(" + std::to_string(rookr) + "," + std::to_string(rookc) + ")\n";
        text += "UP\n";
        text += "MOVE(" + std::to_string(rooknr) + "," + std::to_string(rooknc) + ")\n";
        text += "DOWN\n";
        // Kingを動かす
        text += std::string("// carry King\n");
        text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
        text += "UP\n";
        text += "MOVE(" + std::to_string(isWhite ? r + CELLSIZE : r - CELLSIZE) + "," + std::to_string(rooknc) + ")\n";
        text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
        text += "DOWN\n";
        text += "AUTOCALIB\n";
        // text += "HOME";
    }
    else
    {
        // text+="HOME";
        text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
        text += "UP\n";
        text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
        text += "DOWN\n";
        text += "AUTOCALIB\n";
        // text += "HOME";
    }

    return text;
}