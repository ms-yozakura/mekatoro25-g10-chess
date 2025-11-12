

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

class ChessGame
{
public:
    // コンストラクタ: 盤面初期化
    ChessGame();

    // メインループの処理
    void initBoard();
    Move ask(bool turnWhite);
    void runGame(); // main関数のロジックを移動
    void printBoard() const;

    // メインループ用の移動
    void makeMove(Move &m);
    void undoMove(Move m);

    // 合法手生成
    std::vector<Move> generateMoves(bool white) const;

    // AI機能
    Move bestMove(bool white);

    // 終了判定
    bool isEnd(bool turnWhite);

    // インターフェース
    // 座標から代数表記に変換する (GUI表示用)
    std::string coordsToAlgebraic(int r, int c) const;
    std::string moveToAlgebratic(Move move) const;

    bool isLegal(Move move, bool turnWhite) const;

    bool algebraicToMove(const std::string &moveString, Move &move);

    void getBoardAsStrings(std::string (&rows)[8]) const;

    // FENから盤面設定
    void initBoardWithStrings(const std::string rows[8]);

    // FENから最善手
    Move getBestMoveFromBoard(const std::string rows[8], bool turnWhite);

    // FENから合法手
    std::vector<Move> getLegalMovesFromBoard(const std::string rows[8], bool turnWhite);

private:
    // 状態をカプセル化 (グローバル変数の廃止)
    Piece board[8][8];
    CastlingRights castlingRights;
    const int MAX_DEPTH = 4; // Minimaxの深さ

    std::pair<int, int> enPassantSquare_; // アンパッサン可能なマス (無効な場合は {-1, -1} など)
    int halfMoveClock_ = 0;               // 半手数（50手ルール導入のため）
    int fullMoveNumber_ = 1;              // プレイされている手番の数 (黒番が終了するたびにインクリメント)

    std::vector<std::string> position_history_; // perprtual check判定用盤面履歴

    // ヘルパー関数
    bool algebraicToCoords(const std::string &alg, int &row, int &col) const;
    std::pair<int, int> findKing(bool white) const;
    bool isKingOnBoard(bool white) const;
    bool isSquareAttacked(int r, int c, bool attackingWhite) const;
    void generateSlidingMoves(int r, int c, bool white, char type, std::vector<Move> &moves) const;

    bool isDrawByThreefoldRepetition(bool turnWhite) const;

    // ★ makeMoveInternal / unmakeMoveInternal のシグネチャ変更 ★
    void makeMoveInternal(Move &m); // Undo情報を書き込むためにポインタ渡しする
    void unmakeMoveInternal(Move m);
    void updateCastlingRights(int r, int c);

    // Minimax
    int evaluate() const;
    int minimax(int depth, bool isMaximizingPlayer, int alpha, int beta);

    std::string getBoardStateFEN(bool turnWhite) const;
};
