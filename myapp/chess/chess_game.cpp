#include "chess_game.hpp"

/**
 * version 3.0
 *
 * FEN記法との相性は○
 * AIはコマ価値/位置価値/チェックボーナスから評価
 */

// -------------------------------------------------------------
// 位置価値テーブル (Piece-Square Tables: PSTs) の定義
// -------------------------------------------------------------

// ポーン：中央支配と積極的な前進を評価
const int PawnTable[8][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},                        // 8段目 (プロモーション)
    {80, 80, 80, 80, 80, 80, 80, 80},                // 7段目 (プロモーション間近: +80)
    {50, 50, 60, 50, 50, 60, 50, 40},                // 6段目 (ポーン前進を強く奨励)
    {40, 40, 30, 60, 60, 30, 20, 20},                // 5段目
    {30, 30, 40, 60, 60, 40, 30, 30},                // 4段目
    {0, 0, 30, 10, 10, 30, 0, 0},                    // 3段目 (中央ポーンに僅かなボーナス)
    {-20, -20, -20, -30, -30, -20, -20, -20},        // 2段目 (初期位置のポーンにペナルティ)
    {-100, -100, -100, -100, -100, -100, -100, -100} // 1段目 (あり得ない)
};

const int KnightTable[8][8] = {
    {-50, -40, -30, -30, -30, -30, -40, -50},
    {-40, -20, 0, 5, 5, 0, -20, -40},
    {-30, 5, 5, 5, 5, 5, 5, -30},
    {-30, 0, 10, 10, 10, 10, 0, -30},
    {-30, 5, 10, 10, 10, 10, 5, -30},
    {-30, 0, 5, 5, 5, 5, 0, -30},
    {-40, -20, 0, 0, 0, 0, -20, -40},
    {-30, -10, -10, -10, -10, -10, -10, -30}};

// ビショップ: 中央向きを評価
const int BishopTable[8][8] = {
    {-20, -10, -10, -10, -10, -10, -10, -20},
    {-10, 0, 0, 0, 0, 0, 0, -10},
    {-10, 0, 5, 10, 10, 5, 0, -10},
    {-10, 5, 10, 15, 15, 10, 5, -10}, // 中央(d4, e4)の斜線上に +15 のボーナス
    {-10, 0, 10, 15, 15, 10, 0, -10},
    {-10, 5, 5, 10, 10, 5, 5, -10},
    {-10, 0, 0, 0, 0, 0, 0, -10},
    {-20, -10, -10, -10, -10, -10, -10, -20}};

// ルーク: 7段目/オープンファイルを評価
const int RookTable[8][8] = {
    {0, 0, 0, 5, 5, 0, 0, 0},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {-5, 0, 0, 0, 0, 0, 0, -5},
    {5, 10, 10, 10, 10, 10, 10, 5}, // 7段目ルークは高得点
    {0, 0, 0, 0, 0, 0, 0, 0}};

// クイーン: 中央を評価
const int QueenTable[8][8] = {
    {-20, -10, -10, -5, -5, -10, -10, -20},
    {-10, 0, 0, 0, 0, 0, 0, -10},
    {-10, 0, 5, 5, 5, 5, 0, -10},
    {-5, 0, 5, 5, 5, 5, 0, -5},
    {0, 0, 5, 5, 5, 5, 0, -5},
    {-10, 5, 5, 5, 5, 5, 0, -10},
    {-10, 0, 5, 0, 0, 0, 0, -10},
    {-20, -10, -10, -5, -5, -10, -10, -20}};

// キング (ミドルゲーム):
const int KingTable[8][8] = {
    // 序中盤の評価: 隅に高いボーナス
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-30, -40, -40, -50, -50, -40, -40, -30},
    {-20, -30, -30, -40, -40, -30, -30, -20},
    {-10, -20, -20, -20, -20, -20, -20, -10},
    {20, 20, 0, 0, 0, 0, 20, 20}, // 2段目のキングは少し安全
    {20, 30, 10, 0, 0, 10, 30, 20}};

const int MATE_SCORE = 99999999;
const int DRAW_SCORE = 0; // 引き分けスコア

// -------------------------------------------------------------
// ChessGameクラス
// -------------------------------------------------------------

// コンストラクタ
ChessGame::ChessGame()
{
    initBoard();
    std::srand(std::time(0));
}

// -------------------------------------------------------------
// ユーティリティ関数
// -------------------------------------------------------------
// ----------------------------------------------------------------------
// 代数表記を座標に変換 (例: "a1" -> r=7, c=0)
// ----------------------------------------------------------------------
bool ChessGame::algebraicToCoords(const std::string &alg, int &row, int &col) const
{
    if (alg.length() != 2)
        return false;

    // ファイル (列) を変換: 'a' -> 0, 'b' -> 1, ...
    char file = std::tolower(alg[0]);
    if (file < 'a' || file > 'h')
        return false;
    col = file - 'a'; // a=0, b=1, ...

    // ランク (行) を変換: '1' -> 7, '2' -> 6, ...
    char rank = alg[1];
    if (rank < '1' || rank > '8')
        return false;
    row = '8' - rank; // '8'-'1'=7, '8'-'8'=0

    return true;
}

void ChessGame::printBoard() const
{
    std::cout << "     a   b   c   d   e   f   g   h\n";
    std::cout << "   ┌───┬───┬───┬───┬───┬───┬───┬───┐\n";
    for (int i = 0; i < 8; i++)
    {
        std::cout << " " << 8 - i << " │";
        for (int j = 0; j < 8; j++)
        {
            char c = board[i][j].type;
            std::string piece_str = (c == '*') ? " " : std::string(1, c);
            std::cout << " " << piece_str << " │";
        }
        std::cout << " " << 8 - i << "\n";
        if (i != 7)
            std::cout << "   ├───┼───┼───┼───┼───┼───┼───┼───┤\n";
        else
            std::cout << "   └───┴───┴───┴───┴───┴───┴───┴───┘\n";
    }
    std::cout << "     a   b   c   d   e   f   g   h\n";
}

// ----------------------------------------------------------------------
// メインループ用の移動 (MoveにUndo情報を記録し、履歴を更新する)
// ----------------------------------------------------------------------
// NOTE: シグネチャを makeMove(Move &m) に変更
void ChessGame::makeMove(Move &m)
{
    // 1. 実際の盤面操作と状態更新、Undo情報の記録
    // makeMoveInternal が Move m の old* フィールドを埋め、盤面/状態を更新する
    makeMoveInternal(m);

    // 2. FEN履歴の更新 (三回繰り返しチェック用)
    // makeMoveInternal実行後、board[m.to]には動かした駒が入っている。
    // 次のターンは、この駒の所有者と反対の色である。
    bool nextTurnWhite = !board[m.to.first][m.to.second].isWhite;
    position_history_.push_back(getBoardStateFEN(nextTurnWhite));
}

// ----------------------------------------------------------------------
// メインループ用の移動解除 (AIが makeMove を使った後、盤面を元に戻すために使用)
// ----------------------------------------------------------------------
void ChessGame::undoMove(Move m)
{
    // 1. 実際の盤面操作と状態の復元
    // m は makeMove 内で完全に Undo 情報が記録された Move オブジェクトである
    unmakeMoveInternal(m);

    // 2. FEN履歴の更新 (最新の状態を削除)
    if (!position_history_.empty())
    {
        position_history_.pop_back();
    }
}

// ----------------------------------------------------------------------
// AI探索専用の移動 (MoveにUndo情報を記録し、状態を更新する)
// ----------------------------------------------------------------------
void ChessGame::makeMoveInternal(Move &m)
{
    int r1 = m.from.first, c1 = m.from.second;
    int r2 = m.to.first, c2 = m.to.second;
    Piece pieceToMove = board[r1][c1];
    bool isWhite = pieceToMove.isWhite;

    // =======================================================
    // 1. Undo情報（現在のゲーム状態）を Move に記録
    // =======================================================
    m.oldCastlingRights = castlingRights;
    m.oldEnPassantSquare = enPassantSquare_;
    m.oldHalfMoveClock = halfMoveClock_;
    m.oldFullMoveNumber = fullMoveNumber_;

    // キャプチャされた駒を記録 (通常/アンパッサンで取得元が異なる)
    // まず、通常キャプチャの可能性から始める (r2, c2)
    m.capturedPiece = board[r2][c2];

    // =======================================================
    // 2. halfMoveClock のリセット判定
    // =======================================================
    // ポーンの移動 または 駒のキャプチャがあればリセット
    if (std::toupper(pieceToMove.type) == 'P' || m.capturedPiece.type != '*')
    {
        halfMoveClock_ = 0;
    }
    else
    {
        halfMoveClock_++;
    }

    // アンパッサンの場合、キャプチャ位置を修正
    if (m.isEnPassant)
    {
        // 捕獲されたポーンは移動先(r2, c2)にはおらず、その手前にある
        int capturedR = isWhite ? r2 + 1 : r2 - 1;

        // m.capturedPiece をアンパッサンで捕獲されるポーンに上書き
        m.capturedPiece = board[capturedR][c2];

        // 敵のポーンを盤面から削除
        board[capturedR][c2] = Piece('*', true);
    }
    // 通常の移動では、r2, c2の駒は次のステップで上書きされるか、m.capturedPieceのまま

    // =======================================================
    // 3. キャスリングの特殊処理
    // =======================================================
    if (m.isCastling)
    {
        // キングの移動は通常移動で処理されるため、ルークの移動のみ行う

        // キングサイド (e1->g1 or e8->g8) : ルークは h から f へ
        if (c2 == c1 + 2)
        {
            int rookC1 = 7; // h列
            int rookC2 = 5; // f列
            board[r2][rookC2] = board[r1][rookC1];
            board[r1][rookC1] = Piece('*', true);
        }
        // クイーンサイド (e1->c1 or e8->c8) : ルークは a から d へ
        else if (c2 == c1 - 2)
        {
            int rookC1 = 0; // a列
            int rookC2 = 3; // d列
            board[r2][rookC2] = board[r1][rookC1];
            board[r1][rookC1] = Piece('*', true);
        }
        // キャスリングの場合、キャスリング権は updateCastlingRights で更新されるためここでは不要
        // halfMoveClockはキング移動なのでリセットされない（既に通常移動として処理済み）
    }

    // =======================================================
    // 4. 通常の駒の移動
    // =======================================================
    board[r2][c2] = pieceToMove;
    board[r1][c1] = Piece('*', true); // 移動元を空にする

    // =======================================================
    // 5. プロモーションの実行
    // =======================================================
    if (m.promotedTo != '*')
    {
        // m.promotedTo に基づいて、色付きの駒の種類をセット
        board[r2][c2].type = isWhite ? std::toupper(m.promotedTo) : std::tolower(m.promotedTo);
        // halfMoveClock はポーン移動で既にリセット済み
    }

    // =======================================================
    // 6. ゲーム状態の更新 (キャスリング権, アンパッサン, フルムーブ)
    // =======================================================

    // A. キャスリング権の更新
    updateCastlingRights(r1, c1);
    updateCastlingRights(r2, c2); // ルークがキャプチャされた場合も更新

    // B. 新しいアンパッサンマスの設定 (ポーンの2マス移動の場合)
    // 以前の enPassantSquare_ は既に m.oldEnPassantSquare に保存済み
    if (std::toupper(pieceToMove.type) == 'P' && std::abs(r1 - r2) == 2)
    {
        // ポーンが2マス移動したら、通過したマスを enPassantSquare_ に設定
        int targetR = isWhite ? r1 - 1 : r1 + 1; // 通過した行
        enPassantSquare_ = {targetR, c1};
    }
    else
    {
        // それ以外の移動では、アンパッサンマスは無効化される
        enPassantSquare_ = {-1, -1};
    }

    // C. フルムーブ数の更新 (黒の移動が終了した場合のみ)
    if (!isWhite)
    {
        fullMoveNumber_++;
    }
}
// ----------------------------------------------------------------------
// AI探索専用の移動解除 (Moveに記録されたUndo情報を使って状態を復元する)
// ----------------------------------------------------------------------
void ChessGame::unmakeMoveInternal(Move m)
{
    int r1 = m.from.first, c1 = m.from.second;
    int r2 = m.to.first, c2 = m.to.second;
    Piece pieceToMove = board[r2][c2]; // 移動後の駒（元に戻す駒）
    bool isWhite = pieceToMove.isWhite;

    // =======================================================
    // 1. 盤面上の駒を元に戻す
    // =======================================================

    // A. r2 の駒 (pieceToMove) を r1 に戻す
    board[r1][c1] = pieceToMove;

    // B. r2 に m.capturedPiece を戻す (通常キャプチャの場合)
    // アンパッサンとキャスリングの場合、r2には空マスを戻す
    if (!m.isEnPassant && !m.isCastling)
    {
        board[r2][c2] = m.capturedPiece;
    }
    else
    {
        board[r2][c2] = Piece('*', true);
    }

    // =======================================================
    // 2. プロモーションのUndo
    // =======================================================
    if (m.promotedTo != '*')
    {
        // r1に戻した駒をPawnに戻す
        board[r1][c1].type = isWhite ? 'P' : 'p';
    }

    // =======================================================
    // 3. 特殊移動のUndo
    // =======================================================

    // A. アンパッサンのUndo
    if (m.isEnPassant)
    {
        // 捕獲されたポーン（m.capturedPiece）を元の位置に戻す
        // キャプチャされたポーンは r2 の真下/真上にいた
        int capturedR = isWhite ? r2 + 1 : r2 - 1;

        // m.capturedPiece (ポーン) を元の位置に戻す
        board[capturedR][c2] = m.capturedPiece;
    }

    // B. キャスリングのUndo
    if (m.isCastling)
    {
        // キングサイド (g1->e1 or g8->e8) : ルークは f から h へ
        if (c2 == c1 + 2)
        {
            int rookC1 = 7;                        // h列
            int rookC2 = 5;                        // f列
            board[r1][rookC1] = board[r1][rookC2]; // ルークを h列に戻す
            board[r1][rookC2] = Piece('*', true);  // f列を空にする
        }
        // クイーンサイド (c1->e1 or c8->e8) : ルークは d から a へ
        else if (c2 == c1 - 2)
        {
            int rookC1 = 0;                        // a列
            int rookC2 = 3;                        // d列
            board[r1][rookC1] = board[r1][rookC2]; // ルークを a列に戻す
            board[r1][rookC2] = Piece('*', true);  // d列を空にする
        }
        // r1, c1 はキングの元の位置、r2, c2 はキングの移動後の位置
    }

    // =======================================================
    // 4. ゲーム状態の復元
    // =======================================================

    // A. フルムーブ数の復元 (黒の移動後にインクリメントされた分を元に戻す)
    fullMoveNumber_ = m.oldFullMoveNumber;

    // B. キャスリング権の復元
    castlingRights = m.oldCastlingRights;

    // C. アンパッサンマスの復元
    enPassantSquare_ = m.oldEnPassantSquare;

    // D. 50手ルールカウンターの復元
    halfMoveClock_ = m.oldHalfMoveClock;
}

// -------------------------------------------------------------
// チェック/メイト判定ヘルパー
// -------------------------------------------------------------

bool ChessGame::isKingOnBoard(bool white) const
{
    // findKing(white) を呼び出して、座標が {-1, -1} でないか確認する
    std::pair<int, int> kingPos = findKing(white);
    return kingPos.first != -1;
}

std::pair<int, int> ChessGame::findKing(bool white) const
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            Piece p = board[r][c];
            if (std::toupper(p.type) == 'K' && p.isWhite == white)
            {
                return {r, c};
            }
        }
    }
    return {-1, -1};
}

bool ChessGame::isSquareAttacked(int r, int c, bool attackingWhite) const
{
    // 1. ナイトによる攻撃チェック
    int knight_moves[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
    for (int i = 0; i < 8; i++)
    {
        int nr = r + knight_moves[i][0];
        int nc = c + knight_moves[i][1];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
        {
            Piece p = board[nr][nc];
            if (p.type != '*' && p.isWhite == attackingWhite && std::toupper(p.type) == 'N')
                return true;
        }
    }

    // 2. キングによる攻撃チェック
    for (int dr = -1; dr <= 1; dr++)
    {
        for (int dc = -1; dc <= 1; dc++)
        {
            if (dr == 0 && dc == 0)
                continue;
            int nr = r + dr;
            int nc = c + dc;
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
            {
                Piece p = board[nr][nc];
                if (p.type != '*' && p.isWhite == attackingWhite && std::toupper(p.type) == 'K')
                    return true;
            }
        }
    }

    // 3. ポーンによる攻撃チェック
    int pawn_dir = attackingWhite ? 1 : -1;
    int pr = r + pawn_dir;
    if (pr >= 0 && pr < 8)
    {
        int capture_cols[] = {c - 1, c + 1};
        for (int pc : capture_cols)
        {
            if (pc >= 0 && pc < 8)
            {
                Piece p = board[pr][pc];
                if (p.type != '*' && p.isWhite == attackingWhite && std::toupper(p.type) == 'P')
                    return true;
            }
        }
    }

    // 4. 直線移動駒 (R, B, Q) による攻撃チェック
    std::vector<std::pair<int, int>> sliding_dirs = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}, // R, Q
        {1, 1},
        {1, -1},
        {-1, 1},
        {-1, -1} // B, Q
    };

    for (const auto &dir : sliding_dirs)
    {
        int dr = dir.first, dc = dir.second;
        int nr = r + dr, nc = c + dc;
        char required_type = ((dr == 0 || dc == 0) && dr != dc) ? 'R' : 'B';

        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
        {
            Piece target = board[nr][nc];
            if (target.type != '*')
            {
                if (target.isWhite == attackingWhite)
                {
                    char upper_type = std::toupper(target.type);
                    if (upper_type == 'Q')
                        return true;
                    if (required_type == 'R' && upper_type == 'R')
                        return true;
                    if (required_type == 'B' && upper_type == 'B')
                        return true;
                }
                break;
            }
            nr += dr;
            nc += dc;
        }
    }
    return false;
}
// ----------------------------------------------------------------------
// 局面のFENを生成 (三回繰り返し判定用)
// ----------------------------------------------------------------------
std::string ChessGame::getBoardStateFEN(bool turnWhite) const
{
    std::string fen = "";

    // 1. 盤面 (Piece Placement)
    for (int r = 0; r < 8; ++r)
    {
        int emptyCount = 0;
        for (int c = 0; c < 8; ++c)
        {
            char pieceType = board[r][c].type;
            if (pieceType == '*')
            {
                emptyCount++;
            }
            else
            {
                if (emptyCount > 0)
                {
                    fen += std::to_string(emptyCount);
                    emptyCount = 0;
                }
                fen += pieceType;
            }
        }
        if (emptyCount > 0)
        {
            fen += std::to_string(emptyCount);
        }
        if (r < 7)
        {
            fen += '/';
        }
    }

    // 2. 手番 (Active Color)
    fen += turnWhite ? " w" : " b";

    // 3. キャスリング権 (Castling Availability)
    std::string castling = "";
    if (!castlingRights.whiteKingMoved)
    {
        if (!castlingRights.whiteRookKSidesMoved)
            castling += 'K'; // 白キングサイド
        if (!castlingRights.whiteRookQSidesMoved)
            castling += 'Q'; // 白クイーンサイド
    }
    if (!castlingRights.blackKingMoved)
    {
        if (!castlingRights.blackRookKSidesMoved)
            castling += 'k'; // 黒キングサイド
        if (!castlingRights.blackRookQSidesMoved)
            castling += 'q'; // 黒クイーンサイド
    }
    fen += " " + (castling.empty() ? "-" : castling);

    // 4. アンパッサンターゲットマス (En Passant Target Square)
    if (enPassantSquare_.first != -1)
    {
        // 座標を代数表記に変換 (例: {2, 0} -> "a6")
        fen += " " + coordsToAlgebraic(enPassantSquare_.first, enPassantSquare_.second);
    }
    else
    {
        fen += " -";
    }

    // Note: 50手ルールとフルムーブ数は三回繰り返し判定に不要なため、ここでは含めない
    // ただし、完全なFENが必要な場合は " " + std::to_string(halfMoveClock_) + " " + std::to_string(fullMoveNumber_) を追加

    return fen;
}
// ----------------------------------------------------------------------
// 三回繰り返しによる引き分け判定
// ----------------------------------------------------------------------
bool ChessGame::isDrawByThreefoldRepetition(bool turnWhite) const
{
    // 現在のFENを取得
    std::string currentFEN = getBoardStateFEN(turnWhite);
    int count = 0;

    // 履歴を逆順に辿って、現在のFENと一致する回数を数える
    // Note: FEN履歴には「手を指した後」の盤面が入っている
    for (const auto &historyFEN : position_history_)
    {
        if (historyFEN == currentFEN)
        {
            count++;
        }
    }
    // 3回一致したら引き分け
    // (historyには既に1回目が格納されているため、count == 3 でOK)
    return count >= 3;
}

// -------------------------------------------------------------
// 合法手生成
// -------------------------------------------------------------

void ChessGame::generateSlidingMoves(int r, int c, bool white, char type, std::vector<Move> &moves) const
{
    std::vector<std::pair<int, int>> directions;
    if (type == 'R' || type == 'Q')
    {
        directions.push_back({1, 0});
        directions.push_back({-1, 0});
        directions.push_back({0, 1});
        directions.push_back({0, -1});
    }
    if (type == 'B' || type == 'Q')
    {
        directions.push_back({1, 1});
        directions.push_back({1, -1});
        directions.push_back({-1, 1});
        directions.push_back({-1, -1});
    }

    for (const auto &dir : directions)
    {
        int dr = dir.first, dc = dir.second;
        int nr = r + dr, nc = c + dc;

        while (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
        {
            Piece target = board[nr][nc];
            if (target.type == '*')
            {
                moves.push_back({{r, c}, {nr, nc}});
            }
            else if (target.isWhite != white)
            {
                moves.push_back({{r, c}, {nr, nc}});
                break;
            }
            else
            {
                break;
            }
            nr += dr;
            nc += dc;
        }
    }
}

// ----------------------------------------------------------------------
// 合法手生成 (Move struct に特殊フラグを設定)
// ----------------------------------------------------------------------
std::vector<Move> ChessGame::generateMoves(bool white) const
{
    std::vector<Move> moves;

    // 暫定的な合法手生成 (ここでは、まだ王手回避のチェックはしない)
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            Piece piece = board[r][c];

            if (piece.type != '*' && piece.isWhite == white)
            {
                // ポーン
                if (std::toupper(piece.type) == 'P')
                {
                    // --- ポーンの移動方向と初期位置の設定 ---
                    int dir = white ? -1 : 1;   // 白:上(-1), 黒:下(+1)
                    int startR = white ? 6 : 1; // 白:2段目(6), 黒:7段目(1)
                    int promoR = white ? 0 : 7; // 白:1段目(0), 黒:8段目(7)

                    // -------------------------------------------------
                    // 1. 前方への1マス移動
                    // -------------------------------------------------
                    int r2 = r + dir;
                    if (r2 >= 0 && r2 < 8 && board[r2][c].type == '*')
                    {
                        // プロモーション判定
                        if (r2 == promoR)
                        {
                            // プロモーション移動: 4種類の駒を生成
                            for (char promo : {'Q', 'R', 'B', 'N'})
                            {
                                // isEnPassant=false, isCastling=false の Move を生成
                                moves.push_back(Move({r, c}, {r2, c}, promo));
                            }
                        }
                        else
                        {
                            // 通常の1マス移動
                            moves.push_back(Move({r, c}, {r2, c}));
                        }

                        // -------------------------------------------------
                        // 2. 前方への2マス移動 (初期位置かつ1マス先も空いている)
                        // -------------------------------------------------
                        if (r == startR)
                        {
                            int r3 = r + 2 * dir;
                            if (board[r3][c].type == '*')
                            {
                                moves.push_back(Move({r, c}, {r3, c}));
                            }
                        }
                    }

                    // -------------------------------------------------
                    // 3. 通常の斜めキャプチャ
                    // -------------------------------------------------
                    for (int dc : {-1, 1}) // 左斜め(-1), 右斜め(+1)
                    {
                        int c2 = c + dc;
                        if (c2 >= 0 && c2 < 8)
                        {
                            Piece target = board[r2][c2];
                            if (target.type != '*' && target.isWhite != white)
                            {
                                // プロモーション判定
                                if (r2 == promoR)
                                {
                                    // プロモーションキャプチャ
                                    for (char promo : {'Q', 'R', 'B', 'N'})
                                    {
                                        moves.push_back(Move({r, c}, {r2, c2}, promo));
                                    }
                                }
                                else
                                {
                                    // 通常のキャプチャ
                                    moves.push_back(Move({r, c}, {r2, c2}));
                                }
                            }

                            // -------------------------------------------------
                            // 4. アンパッサンキャプチャ ★NEW★
                            // -------------------------------------------------
                            // 移動先マス(r2, c2)がアンパッサンターゲットマスと一致するか
                            if (enPassantSquare_.first == r2 && enPassantSquare_.second == c2)
                            {
                                // アンパッサンはプロモーションと同時に起こらない
                                // isEnPassant = true の Move を生成
                                moves.push_back(Move({r, c}, {r2, c2}, '*', true, false));
                            }
                        }
                    }
                }
                // キング
                else if (std::toupper(piece.type) == 'K')
                {
                    // キングの移動ロジック
                    // 1マス移動
                    for (int dr = -1; dr <= 1; dr++)
                    {
                        for (int dc = -1; dc <= 1; dc++)
                        {
                            if (dr == 0 && dc == 0)
                                continue;
                            int nr = r + dr, nc = c + dc;
                            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
                            {
                                Piece target = board[nr][nc];
                                if (target.type == '*' || target.isWhite != white)
                                {
                                    moves.push_back({{r, c}, {nr, nc}});
                                }
                            }
                        }
                    }

                    // -------------------------------------------------
                    // 5. キャスリングの移動生成 ★NEW / UPDATE★
                    // キャスリングは、後の王手チェックで合法性が判断されるが、
                    // ここでは移動自体を生成する
                    // -------------------------------------------------

                    // キングの初期位置 (e1 or e8)
                    if ((white && r == 7 && c == 4) || (!white && r == 0 && c == 4))
                    {
                        // キャスリングは、キング、ルークが動いていない、
                        // 間のマスが空いている、通過するマスがチェックされていない
                        // などの条件が複雑だが、ここではまず移動自体を生成する。
                        // (王手チェックは後のisLegalで行う)

                        // 5-1. キングサイド (e->g)
                        // hルークが動いていない and f, gが空
                        bool canKS = white ? !castlingRights.whiteRookKSidesMoved && !castlingRights.whiteKingMoved : !castlingRights.blackRookKSidesMoved && !castlingRights.blackKingMoved;
                        if (canKS && board[r][5].type == '*' && board[r][6].type == '*')
                        { // ★ 追加: f-square (c=5) と g-square (c=6) が攻撃されていないかチェック ★
                            if (!isSquareAttacked(r, 5, !white) && !isSquareAttacked(r, 6, !white))
                            {
                                moves.push_back(Move({r, c}, {r, 6}, '*', false, true));
                            }
                        }

                        // 5-2. クイーンサイド (e->c)
                        // aルークが動いていない and b, c, dが空
                        bool canQS = white ? !castlingRights.whiteRookQSidesMoved && !castlingRights.whiteKingMoved : !castlingRights.blackRookQSidesMoved && !castlingRights.blackKingMoved;
                        if (canQS && board[r][3].type == '*' && board[r][2].type == '*' && board[r][1].type == '*')
                        { // ★ 追加: c-square (c=2) と d-square (c=3) が攻撃されていないかチェック ★
                            if (!isSquareAttacked(r, 3, !white) && !isSquareAttacked(r, 2, !white))
                            {
                                moves.push_back(Move({r, c}, {r, 2}, '*', false, true));
                            }
                        }
                    }
                }
                else if (std::toupper(piece.type) == 'N')
                {
                    // ... ナイトの移動ロジック
                    int knight_moves[8][2] = {{2, 1}, {2, -1}, {-2, 1}, {-2, -1}, {1, 2}, {1, -2}, {-1, 2}, {-1, -2}};
                    for (int i = 0; i < 8; i++)
                    {
                        int nr = r + knight_moves[i][0], nc = c + knight_moves[i][1];
                        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
                        {
                            Piece target = board[nr][nc];
                            if (target.type == '*' || target.isWhite != white)
                            {
                                moves.push_back({{r, c}, {nr, nc}});
                            }
                        }
                    }
                }
                else if (std::toupper(piece.type) == 'R' || std::toupper(piece.type) == 'B' || std::toupper(piece.type) == 'Q')
                {
                    generateSlidingMoves(r, c, white, std::toupper(piece.type), moves);
                }
            }
        }
    } // -------------------------------------------------
    // 王手回避チェック (高速化のため、make/unmake ペアを使用)
    // -------------------------------------------------
    std::vector<Move> validMoves;

    // constメソッド内で状態を変更できないため、thisポインタの定数性を一時的にキャストして解除し、
    // 内部関数（make/unmake）を呼び出せるようにします。
    // ※これはC++の制約を回避する一般的な手法ですが、注意が必要です。
    ChessGame *nonConstThis = const_cast<ChessGame *>(this);

    for (const auto &move : moves)
    {
        // 1. Moveをコピー (Undo情報記録用)
        Move tempMove = move;

        // 2. 状態を進める (Undo情報が tempMove に記録される)
        // const_cast経由で非constの内部関数を呼び出す
        nonConstThis->makeMoveInternal(tempMove);

        // 3. 移動後のキングの位置を取得
        std::pair<int, int> kingPos = nonConstThis->findKing(white);

        // 4. 移動後にキングが王手になっていないかチェック
        // isSquareAttackedの第3引数: 攻撃側（敵）の色
        if (!nonConstThis->isSquareAttacked(kingPos.first, kingPos.second, !white))
        {
            validMoves.push_back(move);
        }

        // 5. 状態を元に戻す (次の擬似合法手のチェックのため)
        nonConstThis->unmakeMoveInternal(tempMove);
    }

    return validMoves;
}

// -------------------------------------------------------------
// AI機能
// -------------------------------------------------------------
int ChessGame::evaluate() const
{

    // 終盤判定
    bool is_endgame = true;
    int pawnCount = 0;
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            char type = std::toupper(board[r][c].type);
            if (type == 'P')
            {
                pawnCount++;
            }
        }
    }
    if (pawnCount > 8)
        is_endgame = false;

    int score = 0;
    // 駒の物質的価値
    const int piece_values[] = {100, 320, 330, 500, 900, 50000};
    const char piece_chars[] = {'P', 'N', 'B', 'R', 'Q', 'K'};

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            Piece p = board[r][c];
            if (p.type == '*')
                continue;

            int material_value = 0;
            char upper_type = std::toupper(p.type);

            // 物質的価値の計算
            for (int i = 0; i < 6; ++i)
            {
                if (piece_chars[i] == upper_type)
                {
                    material_value = piece_values[i];
                    break;
                }
            }

            // ------------------------------------------------
            // ★位置的価値 (Positional Score) の計算 (PSTsの使用)
            // ------------------------------------------------
            int positional_bonus = 0;

            switch (upper_type)
            {
            case 'P':
                positional_bonus = p.isWhite ? PawnTable[r][c] : PawnTable[7 - r][c];
                break;
            case 'N':
                positional_bonus = p.isWhite ? KnightTable[r][c] : KnightTable[7 - r][c];
                break;
            case 'B':
                positional_bonus = p.isWhite ? BishopTable[r][c] : BishopTable[7 - r][c];
                break;
            case 'R':
                positional_bonus = p.isWhite ? RookTable[r][c] : RookTable[7 - r][c];
                break;
            case 'Q':
                positional_bonus = p.isWhite ? QueenTable[r][c] : QueenTable[7 - r][c];
                break;
            case 'K':
                positional_bonus = p.isWhite ? KingTable[r][c] : KingTable[7 - r][c];

                if (is_endgame)
                {
                    // 終盤でキングが中央に出るように評価を**反転**させる (暫定的な対応)
                    // キングの安全性よりも活動性を優先するため
                    positional_bonus = -positional_bonus;
                }
                break;
            }

            if (p.isWhite)
            {
                // 白: スコアに加算
                score += material_value + positional_bonus;
            }
            else
            {
                // 黒: スコアから減算
                score -= (material_value + positional_bonus);
            }
        }
    }

    // ★★★ 終盤のキング安全性ボーナス (汎用的な記述) ★★★
    //-------------------------------------------

    // White's Attack Score (白が黒キングを攻撃)
    std::pair<int, int> blackKingPos = findKing(false);
    int white_attack_on_black = 0;

    // 相手キング周辺のマス(5x5エリア)が攻撃されているかチェック
    for (int dr = -2; dr <= 2; dr++)
    {
        for (int dc = -2; dc <= 2; dc++)
        {
            int nr = blackKingPos.first + dr;
            int nc = blackKingPos.second + dc;
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
            {
                // 黒キング周辺のマスを白（攻撃側）が攻撃しているか
                if (isSquareAttacked(nr, nc, true))
                {
                    white_attack_on_black += 5;
                }
            }
        }
    }

    // Black's Attack Score (黒が白キングを攻撃)
    std::pair<int, int> whiteKingPos = findKing(true);
    int black_attack_on_white = 0;

    for (int dr = -2; dr <= 2; dr++)
    {
        for (int dc = -2; dc <= 2; dc++)
        {
            int nr = whiteKingPos.first + dr;
            int nc = whiteKingPos.second + dc;
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8)
            {
                // 白キング周辺のマスを黒（攻撃側）が攻撃しているか
                if (isSquareAttacked(nr, nc, false))
                {
                    black_attack_on_white += 5;
                }
            }
        }
    }

    // 攻撃ボーナスのウェイト調整
    int weight = is_endgame ? 1 : 2;

    // 最終スコアに反映:
    // 白の攻撃ボーナスは score にプラス (白の有利)
    score += white_attack_on_black * weight;

    // 黒の攻撃ボーナスは score からマイナス (黒の有利 = 白の不利)
    score -= black_attack_on_white * weight;

    // ★★★ 終盤のポーンプロモーションの脅威 ★★★
    //-------------------------------------------
    int passed_pawn_bonus = 0;

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            Piece p = board[r][c];
            if (std::toupper(p.type) == 'P')
            {
                bool isWhite = p.isWhite;
                bool isPassed = true;

                // ポーンの進行方向 (白は上: -1, 黒は下: +1)
                int dir = isWhite ? -1 : 1;

                // ポーンのいるファイル(c)とその左右のファイル(c-1, c+1)をチェック
                for (int check_c = c - 1; check_c <= c + 1; check_c++)
                {
                    if (check_c < 0 || check_c > 7)
                        continue;

                    // ポーンの前方すべてのマスをチェック
                    for (int check_r = r + dir; check_r != (isWhite ? -1 : 8); check_r += dir)
                    {
                        Piece target = board[check_r][check_c];
                        // 敵のポーンが前方にいれば、Passed Pawnではない
                        if (std::toupper(target.type) == 'P' && target.isWhite != isWhite)
                        {
                            isPassed = false;
                            break;
                        }
                    }
                    if (!isPassed)
                        break;
                }

                if (isPassed)
                {
                    // 昇格に近いほど大きなボーナスを与える
                    // 白: r=0 (1段目) に近いほど高得点。黒: r=7 (8段目) に近いほど高得点。
                    int rank_dist = isWhite ? (7 - r) : r; // 1段目から数えて何段目か (r=7/0で0, r=0/7で7)
                    // 10 + rank_dist * 20 程度のボーナス
                    int bonus = 10 + rank_dist * 20;

                    passed_pawn_bonus += isWhite ? bonus : -bonus;
                }
            }
        }
    }
    score += passed_pawn_bonus;

    return score;
}

// ----------------------------------------------------------------------
// Minimaxアルゴリズム (Alpha-Beta枝刈り付き)
// ----------------------------------------------------------------------
int ChessGame::minimax(int depth, bool isMaximizingPlayer, int alpha, int beta)
{
    // =======================================================
    // 1. 基本ケース (Base Cases)
    // =======================================================
    if (depth == 0)
    {
        // 探索深さに達したら評価値を返す
        return evaluate();
    }

    // 50手ルールによる引き分け判定
    // halfMoveClock_ は makeMoveInternal で更新されている
    if (halfMoveClock_ >= 100)
    {
        return DRAW_SCORE;
    }

    // 三回繰り返しによる引き分け判定
    // isDrawByThreefoldRepetition は history_ をチェックするため、この関数が
    // 正しく実装されている必要があります。
    if (isDrawByThreefoldRepetition(isMaximizingPlayer))
    {
        return DRAW_SCORE;
    }

    // ---------------------------------------------
    // 手の生成
    // ---------------------------------------------
    // isMaximizingPlayer = true の場合、現在のターンプレイヤー（白/黒）として生成
    std::vector<Move> possibleMoves = generateMoves(isMaximizingPlayer);

    // メイト/ステイルメイト判定
    if (possibleMoves.empty())
    {
        std::pair<int, int> kingPos = findKing(isMaximizingPlayer);
        bool isCheck = isSquareAttacked(kingPos.first, kingPos.second, !isMaximizingPlayer);

        if (isCheck)
        {
            // チェックメイト (現在のプレイヤーは負け)
            // 評価値は、depthが深いほどメイトまでの手数が短いことを示すように補正する
            return isMaximizingPlayer ? (-MATE_SCORE + (MAX_DEPTH - depth)) : (MATE_SCORE - (MAX_DEPTH - depth));
        }
        else
        {
            // ステイルメイト (引き分け)
            return DRAW_SCORE;
        }
    }

    // =======================================================
    // 2. 最大化プレイヤー (Maximizer: Whiteの番を想定)
    // =======================================================
    if (isMaximizingPlayer)
    {
        int maxEval = -MATE_SCORE; // 非常に低い値で初期化

        for (const auto &move : possibleMoves)
        {
            Move currentMove = move; // Moveをコピー (Undo情報記録用)

            // 状態を進める (historyは更新しない makeMoveInternal を使用)
            makeMoveInternal(currentMove);

            // 再帰探索 (次は最小化プレイヤー)
            int eval = minimax(depth - 1, false, alpha, beta);

            // 状態を元に戻す
            unmakeMoveInternal(currentMove);

            // スコア更新
            maxEval = std::max(maxEval, eval);

            // ★ Alpha更新: 見つけた最善のスコアでαを更新 ★
            alpha = std::max(alpha, maxEval);

            // ★ Beta枝刈り: この枝の最小スコア (β) が、既に他の枝で見つけた最大スコア (α) 以下の場合、
            // これ以上探索してもより良い結果は見つからないため、探索を打ち切る
            if (beta <= alpha)
            {
                break;
            }
        }
        return maxEval;
    }
    // =======================================================
    // 3. 最小化プレイヤー (Minimizer: Blackの番を想定)
    // =======================================================
    else
    {
        int minEval = MATE_SCORE; // 非常に高い値で初期化

        for (const auto &move : possibleMoves)
        {
            Move currentMove = move;

            // 状態を進める
            makeMoveInternal(currentMove);

            // 再帰探索 (次は最大化プレイヤー)
            int eval = minimax(depth - 1, true, alpha, beta);

            // 状態を元に戻す
            unmakeMoveInternal(currentMove);

            // スコア更新
            minEval = std::min(minEval, eval);

            // ★ Beta更新: 見つけた最善のスコアでβを更新 ★
            beta = std::min(beta, minEval);

            // ★ Alpha枝刈り: この枝の最大スコア (α) が、既に他の枝で見つけた最小スコア (β) 以上の場合、
            // これ以上探索してもより良い結果は見つからないため、探索を打ち切る
            if (beta <= alpha)
            {
                break;
            }
        }
        return minEval;
    }
}

Move ChessGame::bestMove(bool white)
{
    // MATE_SCORE が定義されていることを前提とする
    int bestScore = white ? -MATE_SCORE : MATE_SCORE;
    Move best_move;

    auto moves = generateMoves(white);
    if (moves.empty())
    {
        return Move();
    }

    std::vector<Move> tiedMoves;

    for (const auto &move : moves)
    {
        Move currentMove = move; // Moveをコピーし、Undo情報を記録する準備

        // 1. 移動を実行 (参照渡しで currentMove に Undo情報が記録される)
        // NOTE: AI探索のルートノードでは、historyを更新する public な makeMove を使用
        makeMove(currentMove);

        // 2. minimax で評価
        // 探索深さ MAX_DEPTH-1、相手の手番として呼び出す
        int score = minimax(MAX_DEPTH - 1, !white, -MATE_SCORE, MATE_SCORE);

        // 3. 移動を元に戻す (Undo情報が記録された currentMove を使用)
        undoMove(currentMove); // public な undoMove を使用

        if (white)
        {
            if (score > bestScore)
            {
                bestScore = score;
                tiedMoves.clear();
                tiedMoves.push_back(move);
            }
            else if (score == bestScore)
            {
                tiedMoves.push_back(move);
            }
        }
        else
        {
            if (score < bestScore)
            {
                bestScore = score;
                tiedMoves.clear();
                tiedMoves.push_back(move);
            }
            else if (score == bestScore)
            {
                tiedMoves.push_back(move);
            }
        }
    }

    if (!tiedMoves.empty())
    {
        return tiedMoves[std::rand() % tiedMoves.size()];
    }
    return best_move;
}

// -------------------------------------------------------------
// メインルーチン (初期化と入力/ゲーム実行)
// -------------------------------------------------------------

void ChessGame::initBoard()
{
    std::string rows[8] = {
        "rnbqkbnr", "pppppppp", "********", "********",
        "********", "********", "PPPPPPPP", "RNBQKBNR"};
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            char c = rows[i][j];
            if (c == '*')
                board[i][j] = Piece('*', true);
            else
                board[i][j] = Piece(c, std::isupper(c));
        }
    }
    castlingRights = {}; // 構造体のリセット
}
// ----------------------------------------------------------------------
// プレイヤーからの入力を受け付け、Moveオブジェクトに変換する
// ----------------------------------------------------------------------
Move ChessGame::ask(bool turnWhite)
{
    std::string moveString;
    Move move = Move(); // デフォルトコンストラクタで初期化

    while (true)
    {
        std::cout << (turnWhite ? "White" : "Black") << " move (e.g., e2e4 or e7e8q): ";
        std::cin >> moveString;

        if (moveString.length() < 4)
        {
            std::cout << "Invalid move format. Try again.\n";
            continue;
        }

        // 1. 移動元・移動先の座標を取得
        int r1, c1, r2, c2;
        std::string fromStr = moveString.substr(0, 2);
        std::string toStr = moveString.substr(2, 2);

        if (!algebraicToCoords(fromStr, r1, c1) || !algebraicToCoords(toStr, r2, c2))
        {
            std::cout << "Invalid square names. Try again.\n";
            continue;
        }

        // 2. プロモーション駒の取得
        char promo = '*';
        if (moveString.length() == 5)
        {
            promo = std::toupper(moveString[4]); // 入力された駒を大文字で取得 (Q, R, B, N)
            if (promo != 'Q' && promo != 'R' && promo != 'B' && promo != 'N')
            {
                std::cout << "Invalid promotion piece. Use q, r, b, or n. Try again.\n";
                continue;
            }
        }

        // 3. 擬似的なMoveオブジェクトを作成
        Move proposedMove({r1, c1}, {r2, c2}, promo);

        // 4. 合法手リストを生成し、一致するMoveを探す
        // generateMovesは、isEnPassantやisCastlingフラグが設定された完全な合法手リストを返す
        std::vector<Move> legalMoves = generateMoves(turnWhite);

        bool found = false;
        for (const auto &legalMove : legalMoves)
        {
            // Moveの比較演算子(operator==)は from, to, promotedTo を比較するように実装済みと仮定
            if (proposedMove == legalMove)
            {
                move = legalMove; // 完全な情報（特殊フラグ）を持つMoveをコピー
                found = true;
                break;
            }
        }

        if (found)
        {
            return move;
        }
        else
        {
            std::cout << "Move is illegal or invalid. Try again.\n";
        }
    }
}

void ChessGame::runGame()
{
    std::cout << "--- Full Chess (Minimax AI): Human (White) vs AI (Black) ---\n";
    std::cout << "AI Depth: " << MAX_DEPTH << " (3-ply search).\n";
    std::cout << "Note: En Passant is NOT implemented. (Promotion and Checkmate/Stalemate are included.)\n";
    printBoard();

    bool turnWhite = true;
    for (int step = 0; step < 100; step++)
    {
        // 1. 合法手を生成し、メイト/ステールメイトを判定
        auto possibleMoves = generateMoves(turnWhite);

        if (possibleMoves.empty())
        {
            std::pair<int, int> kingPos = findKing(turnWhite);
            bool isCheck = isSquareAttacked(kingPos.first, kingPos.second, !turnWhite);

            if (isCheck)
            {
                std::cout << "\n*** CHECKMATE! " << (!turnWhite ? "White" : "Black") << " WINS! ***\n";
            }
            else
            {
                std::cout << "\n*** STALEMATE! Game is a DRAW. ***\n";
            }
            break;
        }

        if (step > 0 && isDrawByThreefoldRepetition(turnWhite))
        {
            std::cout << "\n*** DRAW! Game is a DRAW by PERPETUAL CHECK. ***\n";
            break;
        }

        bool opponentWhite = !turnWhite;
        if (!isKingOnBoard(opponentWhite))
        {
            // 相手（捕獲された側）のキングが消えた
            std::cout << "\n*** FATAL ERROR: KING CAPTURED! " << (turnWhite ? "White" : "Black") << " WINS! ***\n";
            std::cout << "NOTE: This should not happen if checkmate/check logic is perfect.\n";
            break;
        }

        // 2. 指し手の取得 (AIとユーザーを分離)
        Move move;
        if (turnWhite)
        {
            move = ask(turnWhite); // ユーザー (白) の手
        }
        else
        {
            std::cout << "AI (Black) is thinking...\n";
            move = bestMove(turnWhite); // AI (黒) の手
        }

        // 3. 指し手の表示、適用、ターン切替
        std::cout << "Move No: "
                  << fullMoveNumber_
                  << " ";
        std::cout << (turnWhite ? "White" : "Black") << " moves: "
                  << char('a' + move.from.second) << 8 - move.from.first
                  << " -> "
                  << char('a' + move.to.second) << 8 - move.to.first
                  << "\n";

        makeMove(move);
        printBoard();
        turnWhite = !turnWhite;
    }

    std::cout << "\nGame finished.\n";
    std::cout << "Final Evaluation (White's perspective): " << evaluate() << std::endl;
}

//! new

// -------------------------------------------------------------
// 引数からboardをリセットするメソッド
// -------------------------------------------------------------

/**
 * グローバル変数 board を引数の盤面設定で初期化する
 * 注意: この関数はグローバルの castlingRights も初期状態にリセットします
 */
void ChessGame::initBoardWithStrings(const std::string rows[8])
{
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            char c = rows[i][j];
            if (c == '*')
                board[i][j] = Piece('*', true);
            else
                board[i][j] = Piece(c, std::isupper(c));
        }
    }
    // キャスリング権を初期状態にリセット (より厳密には引数で受け取るべき)
    castlingRights = {};
}

// -------------------------------------------------------------
// 最善手を取得するメソッド
// -------------------------------------------------------------

/**
 * 外部の盤面設定に基づいて、指定された手番の最善手を返す
 * @param rows 盤面設定の文字列配列 (例: "rnbqkbnr", "pppppppp", ...)
 * @param turnWhite 最善手を計算する手番 (true: White, false: Black)
 * @return 最善手 Move 構造体
 */
Move ChessGame::getBestMoveFromBoard(const std::string rows[8], bool turnWhite)
{
    // 1. グローバルの board を引数の盤面で一時的に設定
    //    (現在の main() の board 状態は上書きされる)
    initBoardWithStrings(rows);

    // 2. 設定された盤面で bestMove を実行
    return bestMove(turnWhite);
}

std::vector<Move> ChessGame::getLegalMovesFromBoard(const std::string rows[8], bool turnWhite)
{
    // 1. グローバルの board を引数の盤面で一時的に設定
    //    (現在の main() の board 状態は上書きされる)
    initBoardWithStrings(rows);

    return generateMoves(turnWhite);
}
// ----------------------------------------------------------------------
// 座標を代数表記に変換 (例: r=7, c=0 -> "a1")
// ----------------------------------------------------------------------
std::string ChessGame::coordsToAlgebraic(int r, int c) const
{
    // C++の座標 (r:0-7, c:0-7) をチェスの表記 (File:a-h, Rank:8-1) に変換
    if (r < 0 || r > 7 || c < 0 || c > 7)
    {
        // 無効な座標の場合はエラー値 (ただしFENでは'-'を使うべき)
        return "-";
    }

    // File (列): 0->a, 1->b, ..., 7->h
    char file = 'a' + c;

    // Rank (行): 0->8, 1->7, ..., 7->1
    char rank = '8' - r;

    return std::string{file} + std::string{rank};
}
std::string ChessGame::moveToAlgebratic(Move move) const
{
    // Move.from.first = 開始行 (r1), Move.from.second = 開始列 (c1)
    int r1 = move.from.first;
    int c1 = move.from.second;

    // Move.to.first = 終了行 (r2), Move.to.second = 終了列 (c2)
    int r2 = move.to.first;
    int c2 = move.to.second;

    // 開始座標を代数表記に変換
    std::string start_alg = coordsToAlgebraic(r1, c1);

    // 終了座標を代数表記に変換
    std::string end_alg = coordsToAlgebraic(r2, c2);

    // 2つを結合して返却 (例: "e2" + "e4" = "e2e4")
    return start_alg + end_alg;
}

bool ChessGame::isLegal(Move move, bool turnWhite) const
{
    std::vector<Move> moves = generateMoves(turnWhite);
    auto it = std::find(moves.begin(), moves.end(), move);

    if (it != moves.end())
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool ChessGame::algebraicToMove(const std::string &moveString, Move &move)
{
    // 入力は 'e2e4' のような4文字を想定
    if (moveString.length() != 4)
    {
        return false;
    }

    // 1. 移動元 (Source) の代数表記を取得: "e2"
    std::string sourceAlg = moveString.substr(0, 2);
    int startRow, startCol;

    // 2. 移動先 (Destination) の代数表記を取得: "e4"
    std::string destAlg = moveString.substr(2, 2);
    int endRow, endCol;

    // 3. 移動元座標の変換
    if (!algebraicToCoords(sourceAlg, startRow, startCol))
    {
        return false;
    }

    // 4. 移動先座標の変換
    if (!algebraicToCoords(destAlg, endRow, endCol))
    {
        return false;
    }

    // 5. Move型に格納
    // Moveは std::pair<std::pair<int,int> (from), std::pair<int,int> (to)>
    move.from = {startRow, startCol}; // 移動元 (行, 列)
    move.to = {endRow, endCol};       // 移動先 (行, 列)

    return true;
}

void ChessGame::getBoardAsStrings(std::string (&rows)[8]) const
{
    for (int i = 0; i < 8; i++) // 行 (0から7)
    {
        // 既存の文字列をクリアしてから使用
        rows[i].clear();
        rows[i].reserve(8); // 8文字分のメモリを確保

        for (int j = 0; j < 8; j++) // 列 (0から7)
        {
            // Piece構造体から駒の種類を示す文字を取得し、追加
            char c = board[i][j].type;
            rows[i].push_back(c);
        }
    }
}
// ----------------------------------------------------------------------
// ゲーム終了判定 (メインループ用)
// ----------------------------------------------------------------------
bool ChessGame::isEnd(bool turnWhite)
{
    // 1. 合法手を生成し、メイト/ステイルメイトを判定
    auto possibleMoves = generateMoves(turnWhite);

    if (possibleMoves.empty())
    {
        std::pair<int, int> kingPos = findKing(turnWhite);
        bool isCheck = isSquareAttacked(kingPos.first, kingPos.second, !turnWhite);

        if (isCheck)
        {
            std::cout << "\n*** CHECKMATE! " << (!turnWhite ? "White" : "Black") << " WINS! ***\n";
        }
        else
        {
            std::cout << "\n*** STALEMATE! Game is a DRAW. ***\n";
        }
        return true;
    }

    // ★ 2. 三回繰り返しによる引き分け判定 ★
    // isDrawByThreefoldRepetitionは既にminimaxに必要なロジックとして提案済み
    if (isDrawByThreefoldRepetition(turnWhite))
    {
        std::cout << "\n*** DRAW! Game is a DRAW by Threefold Repetition. ***\n";
        return true;
    }

    // ★ 3. 50手ルールによる引き分け判定 ★
    // halfMoveClock_ は makeMoveInternal/unmakeMoveInternal で管理されている
    if (halfMoveClock_ >= 100)
    {
        std::cout << "\n*** DRAW! Game is a DRAW by 50-move Rule. ***\n";
        return true;
    }

    // 終了条件を満たさない場合は続行
    return false;
}
// ----------------------------------------------------------------------
// キャスリング権の更新
// ----------------------------------------------------------------------

void ChessGame::updateCastlingRights(int r1, int c1)
{
    if (r1 == 7)
    { // 白
        if (c1 == 4)
            castlingRights.whiteKingMoved = true;
        else if (c1 == 0)
            castlingRights.whiteRookQSidesMoved = true;
        else if (c1 == 7)
            castlingRights.whiteRookKSidesMoved = true;
    }
    else if (r1 == 0)
    { // 黒
        if (c1 == 4)
            castlingRights.blackKingMoved = true;
        else if (c1 == 0)
            castlingRights.blackRookQSidesMoved = true;
        else if (c1 == 7)
            castlingRights.blackRookKSidesMoved = true;
    }
}

bool ChessGame::isPromotionMove(Move move)
{
    Piece piece = board[move.from.first][move.from.second];
    bool isWhite = (piece.type == std::toupper(piece.type));

    int promoR = isWhite ? 0 : 7; // 白:1段目(0), 黒:8段目(7)

    return (std::toupper(piece.type) == 'P' && move.to.first == promoR);
}