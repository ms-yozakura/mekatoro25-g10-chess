#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>

// CastlingRights 構造体を chess_game.hpp から移動
struct CastlingRights
{
    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    bool whiteRookQSidesMoved = false; // クイーンサイド (a1/a8)
    bool whiteRookKSidesMoved = false; // キングサイド (h1/h8)
    bool blackRookQSidesMoved = false;
    bool blackRookKSidesMoved = false;

    // 比較演算子（FENヒストリなどにも役立つため追加）
    bool operator==(const CastlingRights &other) const
    {
        return whiteKingMoved == other.whiteKingMoved &&
               blackKingMoved == other.blackKingMoved &&
               whiteRookQSidesMoved == other.whiteRookQSidesMoved &&
               whiteRookKSidesMoved == other.whiteRookKSidesMoved &&
               blackRookQSidesMoved == other.blackRookQSidesMoved &&
               blackRookKSidesMoved == other.blackRookKSidesMoved;
    }
};

struct Piece
{
    char type; // K, Q, R, B, N, P (大文字=白、小文字=黒)
    bool isWhite;
    Piece(char t = '*', bool white = true) : type(t), isWhite(white) {}
};

struct Move
{
    std::pair<int, int> from;
    std::pair<int, int> to;

    // ----------------------------------------------------
    // 移動の特性 (移動そのものの情報)
    // ----------------------------------------------------
    char promotedTo = '*';
    bool isEnPassant = false;
    bool isCastling = false;
    Piece capturedPiece = Piece('*', true); // キャプチャされた駒（デフォルトは空マス）

    // ----------------------------------------------------
    // 状態のロギング (Undoのための情報)
    // makeMove実行時に、この移動前の状態を記録する
    // ----------------------------------------------------
    CastlingRights oldCastlingRights;                  // 移動前のキャスリング権
    std::pair<int, int> oldEnPassantSquare = {-1, -1}; // 移動前のアンパッサンマス
    int oldHalfMoveClock = 0;                          // 移動前の50手ルールカウンター
    int oldFullMoveNumber = 1;                         // 移動前のフルムーブ数

    // デフォルトコンストラクタ
    Move() = default;

    // 基本移動コンストラクタ (合法手生成時に使用)
    Move(std::pair<int, int> f, std::pair<int, int> t,
         char promo = '*', bool ep = false, bool cs = false)
        : from(f), to(t), promotedTo(promo), isEnPassant(ep), isCastling(cs) {}

    // 比較演算子 (Moveの同一性を判断)
    bool operator==(const Move &other) const
    {
        // Undo情報はMoveの同一性に関わらないため、比較しない
        return from == other.from &&
               to == other.to &&
               promotedTo == other.promotedTo &&
               isEnPassant == other.isEnPassant &&
               isCastling == other.isCastling;
    }
};