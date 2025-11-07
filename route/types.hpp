#pragma once


#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>

struct Piece {
    char type;    // K, Q, R, B, N, P (大文字=白、小文字=黒)
    bool isWhite;
    Piece(char t = '*', bool white = true) : type(t), isWhite(white) {}
};


// 移動を表す型エイリアス
using Move = std::pair<std::pair<int,int>, std::pair<int,int>>;