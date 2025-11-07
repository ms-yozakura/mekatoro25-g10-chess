

//+++
// made with Gemini
// ・terminal上で動くチェスプログラム
// ・ChessGameクラスで管理
// ・minimax法、コマ自体の価値と位置価値テーブルを考慮
// ・プロモーション/キャスリング（アンパサンなし）
// ・ステイルメイトorチェックメイト勝利、パーペチュアルチェックで引き分け
//      （バグ回避のためキング奪取による勝利も実装）
//+++

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <utility>
#include <cstdlib>
#include <ctime>
#include <cctype>
#include <algorithm>

#include "types.hpp"

// キャスリング判定のための移動履歴
struct CastlingRights
{
    bool whiteKingMoved = false;
    bool blackKingMoved = false;
    bool whiteRookQSidesMoved = false; // クイーンサイド (a1/a8)
    bool whiteRookKSidesMoved = false; // キングサイド (h1/h8)
    bool blackRookQSidesMoved = false;
    bool blackRookKSidesMoved = false;
};

class ChessGame
{
public:
    // コンストラクタ: 盤面初期化
    ChessGame();

    // メインループの処理
    void initBoard();
    Move ask(bool turnWhite);
    void runGame(); // main関数のロジックを移動

    // ユーティリティ/盤面操作 (constを付けて状態を変更しないことを明示)
    void printBoard() const;

    // メインループ用の移動 (CastlingRightsを更新する)
    void makeMove(Move m);

    // ゲーム判定
    std::vector<Move> generateMoves(bool white) const;

    // AI機能
    Move bestMove(bool white);

    // FENから盤面設定
    void initBoardWithStrings(const std::string rows[8]);

    // FENから最善手
    Move getBestMoveFromBoard(const std::string rows[8], bool turnWhite);

    // FENから合法手
    std::vector<Move> getLegalMovesFromBoard(const std::string rows[8], bool turnWhite);

    // 座標から代数表記に変換する (GUI表示用)
    std::string coordsToAlgebraic(int r, int c) const;
    std::string moveToAlgebratic(Move move) const;

    bool isLegal(Move move, bool turnWhite) const;

    bool algebraicToMove(const std::string &moveString, Move &move);

    void getBoardAsStrings(std::string (&rows)[8]) const;

    bool isEnd(bool turnWhite);

private:
    // 状態をカプセル化 (グローバル変数の廃止)
    Piece board[8][8];
    CastlingRights castlingRights;
    const int MAX_DEPTH = 4; // Minimaxの深さ

    std::vector<std::string> position_history_; // perprtual check判定用盤面履歴

    // ヘルパー関数
    bool algebraicToCoords(const std::string &alg, int &row, int &col) const;
    void updateCastlingRights(int r1, int c1);
    std::pair<int, int> findKing(bool white) const;
    bool isKingOnBoard(bool white) const;
    bool isSquareAttacked(int r, int c, bool attackingWhite) const;
    void generateSlidingMoves(int r, int c, bool white, char type, std::vector<Move> &moves) const;

    std::string getBoardStateFEN(bool turnWhite) const;

    bool isDrawByThreefoldRepetition(bool turnWhite) const;

    // AI探索専用の移動 (CastlingRightsは更新しない)
    void makeMoveInternal(Move m, Piece &capturedPiece);
    void unmakeMoveInternal(Move m, Piece capturedPiece);

    // Minimax
    int evaluate() const;
    int minimax(int depth, bool isMaximizingPlayer, int alpha, int beta);
};
