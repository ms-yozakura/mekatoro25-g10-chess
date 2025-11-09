#include "chess_game.hpp"

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
    {-30, 0, 5000, 5, 5, 5, 0, -30},
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

bool ChessGame::algebraicToCoords(const std::string &alg, int &row, int &col) const
{
    if (alg.length() != 2)
        return false;
    col = alg[0] - 'a';
    row = 7 - (alg[1] - '1');
    return col >= 0 && col < 8 && row >= 0 && row < 8;
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

// メインループ用 (CastlingRightsを更新)
void ChessGame::makeMove(Move m)
{
    // makeMoveInternalのロジックを使用し、CastlingRightsの更新を追加
    int r1 = m.first.first, c1 = m.first.second;
    int r2 = m.second.first, c2 = m.second.second;

    // キャスリングの特殊処理
    if (std::toupper(board[r1][c1].type) == 'K' && std::abs(c1 - c2) == 2)
    {
        // ... (キャスリングの移動ロジック - makeMoveInternalとほぼ同じ)
        if (c2 > c1)
        { // キングサイド
            board[r2][c2] = board[r1][c1];
            board[r1][c1] = Piece('*', true);
            board[r2][c2 - 1] = board[r1][7];
            board[r1][7] = Piece('*', true);
        }
        else
        { // クイーンサイド
            board[r2][c2] = board[r1][c1];
            board[r1][c1] = Piece('*', true);
            board[r2][c2 + 1] = board[r1][0];
            board[r1][0] = Piece('*', true);
        }
    }
    else
    { // 通常の移動
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = Piece('*', true);

        // 移動後、ポーンの昇格をチェックする
        if (std::toupper(board[r2][c2].type) == 'P')
        {
            bool isWhite = board[r2][c2].isWhite;
            // 白: 0行目 (1段目)、黒: 7行目 (8段目)
            if ((isWhite && r2 == 0) || (!isWhite && r2 == 7))
            {
                // クイーンに昇格
                board[r2][c2].type = isWhite ? 'Q' : 'q';
            }
        }
    }
    updateCastlingRights(r1, c1);

    position_history_.push_back(getBoardStateFEN(board[r2][c2].isWhite));
}

// AI探索用
void ChessGame::makeMoveInternal(Move m, Piece &capturedPiece)
{
    int r1 = m.first.first, c1 = m.first.second;
    int r2 = m.second.first, c2 = m.second.second;

    capturedPiece = board[r2][c2];

    // キャスリングの特殊処理
    if (std::toupper(board[r1][c1].type) == 'K' && std::abs(c1 - c2) == 2)
    {
        if (c2 > c1)
        { // キングサイド
            board[r2][c2] = board[r1][c1];
            board[r1][c1] = Piece('*', true);
            board[r2][c2 - 1] = board[r1][7];
            board[r1][7] = Piece('*', true);
        }
        else
        { // クイーンサイド
            board[r2][c2] = board[r1][c1];
            board[r1][c1] = Piece('*', true);
            board[r2][c2 + 1] = board[r1][0];
            board[r1][0] = Piece('*', true);
        }
        capturedPiece.type = 'C';
    }
    // キャスリング以外の場合
    else
    {
        board[r2][c2] = board[r1][c1];
        board[r1][c1] = Piece('*', true);

        // プロモーションをチェック
        if (std::toupper(board[r2][c2].type) == 'P')
        {
            bool isWhite = board[r2][c2].isWhite;
            // 白: 0行目 (1段目)、黒: 7行目 (8段目)
            if ((isWhite && r2 == 0) || (!isWhite && r2 == 7))
            {
                // クイーンに昇格
                board[r2][c2].type = isWhite ? 'Q' : 'q';

                // ★★★ 修正: プロモーションがあったことを記録 ★★★
                if (capturedPiece.type == '*')
                { // キャプチャがない場合のみ
                    // 'X'はプロモーションを示すフラグとして利用
                    capturedPiece.type = 'X';
                }
            }
        }
    }
}

// AI探索用 Undo
void ChessGame::unmakeMoveInternal(Move m, Piece capturedPiece)
{
    int r1 = m.first.first, c1 = m.first.second;
    int r2 = m.second.first, c2 = m.second.second;

    if (capturedPiece.type == 'C')
    { // キャスリングのUndo
        if (c2 > c1)
        { // キングサイド
            board[r1][c1] = board[r2][c2];
            board[r2][c2] = Piece('*', true);
            board[r1][7] = board[r2][c2 - 1];
            board[r2][c2 - 1] = Piece('*', true);
        }
        else
        { // クイーンサイド
            board[r1][c1] = board[r2][c2];
            board[r2][c2] = Piece('*', true);
            board[r1][0] = board[r2][c2 + 1];
            board[r2][c2 + 1] = Piece('*', true);
        }
    } // ★★★ 修正: プロモーションのUndoをチェック ★★★
    else if (capturedPiece.type == 'X')
    {
        // 1. プロモーションでクイーンになった駒を元の位置 r1 に戻す
        board[r1][c1] = board[r2][c2];

        // 2. r1に戻された駒をポーンに戻す
        board[r1][c1].type = board[r1][c1].isWhite ? 'P' : 'p';

        // 3. r2にはキャプチャされた駒（オリジナル）がないため、空を戻す
        board[r2][c2] = Piece('*', true);
    }
    // ★★★ 修正: 通常のUndo処理（ポーンのキャプチャも含む） ★★★
    else
    {
        // r2の駒をr1に戻す
        board[r1][c1] = board[r2][c2];

        // r2にキャプチャされた駒（ポーン 'p' を含む）を戻す
        board[r2][c2] = capturedPiece;
    }
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

// -------------------------------------------------------------
// 履歴保存と三回反復チェック
// -------------------------------------------------------------

std::string ChessGame::getBoardStateFEN(bool turnWhite) const
{
    std::string fen = "";
    // 1. 駒の配置 (board[8][8] を 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR' の形式に変換)
    //    ★この部分の実装が少し大変ですが、盤面をユニークに識別できます。
    //    今回は簡易的に、全マスを繋げた文字列に色と権利情報を追加するだけでも可。
    // 例: "rnbqkbnr...PPPPPPPP" + (turnWhite ? "w" : "b") + castling_rights_string

    // 2. 手番 (w or b) を追加
    // 3. キャスリング権 (KQkqなど) を追加
    // 4. アンパッサンターゲットマス (今回は省略可) を追加
    // 5. 50手ルールカウント (今回は省略可) を追加
    // 6. フルムーブ数 (今回は省略可) を追加

    // 簡易版の例 (駒の配置と手番のみ):
    for (int r = 0; r < 8; ++r)
    {
        for (int c = 0; c < 8; ++c)
        {
            fen += board[r][c].type;
        }
    }
    fen += (turnWhite ? "w" : "b");
    return fen;
}

// chess_game.cpp に追加
bool ChessGame::isDrawByThreefoldRepetition(bool turnWhite) const
{
    if (position_history_.empty())
        return false;

    // 現在の盤面状態を取得
    std::string current_state = getBoardStateFEN(turnWhite);
    int count = 0;

    // 履歴を逆順にチェックして、同じ状態が何回出現したか数える
    for (const auto &state : position_history_)
    {
        if (state == current_state)
        {
            count++;
        }
    }

    // 3回以上出現したら引き分け
    return count >= 3;
}

// -------------------------------------------------------------
// 合法手生成
// -------------------------------------------------------------

void ChessGame::generateSlidingMoves(int r, int c, bool white, char type, std::vector<Move> &moves) const
{
    // ... (元の generateSlidingMoves のロジックをそのまま移植)
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

std::vector<Move> ChessGame::generateMoves(bool white) const
{
    std::vector<Move> all_moves;

    // 1. 全ての駒について**形式的に**動ける手を生成 (キャスリングを含む)
    // (元の generateMoves の前半ロジックを移植)
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            Piece p = board[r][c];
            if (p.type == '*' || p.isWhite != white)
                continue;

            char type = std::toupper(p.type);
            int dir = white ? -1 : 1;

            if (type == 'P')
            {
                // ... ポーンの移動ロジック
                int ni = r + dir;
                if (ni >= 0 && ni < 8 && board[ni][c].type == '*')
                {
                    all_moves.push_back({{r, c}, {ni, c}});
                }
                bool isInitialPos = (white && r == 6) || (!white && r == 1);
                int ni2 = r + 2 * dir;
                int ni1 = r + dir;
                if (isInitialPos && ni2 >= 0 && ni2 < 8 && board[ni1][c].type == '*' && board[ni2][c].type == '*')
                {
                    all_moves.push_back({{r, c}, {ni2, c}});
                }
                int capture_cols[] = {c - 1, c + 1};
                for (int nc : capture_cols)
                {
                    if (ni >= 0 && ni < 8 && nc >= 0 && nc < 8)
                    {
                        Piece target = board[ni][nc];
                        if (target.type != '*' && target.isWhite != white)
                        {
                            all_moves.push_back({{r, c}, {ni, nc}});
                        }
                    }
                }
            }
            else if (type == 'K')
            {
                // ... キングの移動ロジック
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
                                all_moves.push_back({{r, c}, {nr, nc}});
                            }
                        }
                    }
                }
                // キャスリング
                int rank = white ? 7 : 0;
                bool king_moved = white ? castlingRights.whiteKingMoved : castlingRights.blackKingMoved;
                if (r == rank && c == 4 && !king_moved)
                {
                    // キングサイド
                    bool rook_ks_moved = white ? castlingRights.whiteRookKSidesMoved : castlingRights.blackRookKSidesMoved;
                    if (!rook_ks_moved && board[rank][5].type == '*' && board[rank][6].type == '*')
                    {
                        all_moves.push_back({{r, c}, {r, 6}});
                    }
                    // クイーンサイド
                    bool rook_qs_moved = white ? castlingRights.whiteRookQSidesMoved : castlingRights.blackRookQSidesMoved;
                    if (!rook_qs_moved && board[rank][1].type == '*' && board[rank][2].type == '*' && board[rank][3].type == '*')
                    {
                        all_moves.push_back({{r, c}, {r, 2}});
                    }
                }
            }
            else if (type == 'N')
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
                            all_moves.push_back({{r, c}, {nr, nc}});
                        }
                    }
                }
            }
            else if (type == 'R' || type == 'B' || type == 'Q')
            {
                generateSlidingMoves(r, c, white, type, all_moves);
            }
        }
    }

    // 2. 違法な手 (自ら王手になる手) の除外処理
    std::vector<Move> safeMoves;
    bool selfWhite = white;
    bool opponentWhite = !white;

    for (const auto &move : all_moves)
    {
        Piece capturedPiece;
        // AI探索用の移動関数を使用
        ChessGame temp_game = *this; // 盤面状態をコピー
        temp_game.makeMoveInternal(move, capturedPiece);

        std::pair<int, int> kingPos = temp_game.findKing(selfWhite);
        int kr = kingPos.first;
        int kc = kingPos.second;

        bool isInCheck = temp_game.isSquareAttacked(kr, kc, opponentWhite);

        if (!isInCheck)
        {
            safeMoves.push_back(move);
        }
        // Undoは不要 (temp_game がスコープを抜けるため)
    }

    return safeMoves;
}

// -------------------------------------------------------------
// AI機能 (Minimax)
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
    // 駒の物質的価値 (P:100, N/B:300, R:500, Q:900, K:1000000)
    const int piece_values[] = {200, 300, 300, 500, 900, 10000000};
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
            const int (*table)[8] = nullptr;

            switch (upper_type)
            {
            case 'P':
                table = PawnTable;
                break;
            case 'N':
                table = KnightTable;
                break;
            case 'B':
                table = BishopTable;
                break;
            case 'R':
                table = RookTable;
                break;
            case 'Q':
                table = QueenTable;
                break;
            case 'K':
                table = KingTable;
                break;
            }

            if (table)
            {
                // 白の駒はそのまま (r, c) を使い、黒の駒は盤面を上下反転して (7-r, c) を使う
                int row_index = p.isWhite ? r : (7 - r);
                positional_bonus = table[row_index][c];

                // ★★★ キングPSTの終盤反転 ★★★
                if (upper_type == 'K' && is_endgame)
                {
                    // 終盤でキングが中央に出るように評価を反転させる
                    positional_bonus = -positional_bonus;
                }
            }
            // ------------------------------------------------

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
                    white_attack_on_black += 10;
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
                    black_attack_on_white += 10;
                }
            }
        }
    }

    // 攻撃ボーナスのウェイト調整
    int weight = is_endgame ? 2 : 1; // 終盤なら攻撃ボーナスを強める

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
    return score;
}

int ChessGame::minimax(int depth, bool isMaximizingPlayer, int alpha, int beta)
{ // 1. 探索深さが0に達した場合
    if (depth == 0)
    {
        return evaluate(); // 駒得・位置的価値で評価
    }

    // 2. 合法手を生成
    // constメソッド generateMoves を呼び出し
    auto moves = generateMoves(isMaximizingPlayer);

    // 3. 葉ノード (チェックメイト or ステールメイト) の判定
    if (moves.empty())
    {
        // 自分のキングの位置を確認
        std::pair<int, int> kingPos = findKing(isMaximizingPlayer);
        // 相手からの攻撃を受けているか？
        bool isCheck = isSquareAttacked(kingPos.first, kingPos.second, !isMaximizingPlayer);

        if (isCheck)
        {
            // チェックメイト！ 最大化側は極めて大きなプラス、最小化側は極めて大きなマイナス
            // キングの価値(1000000000)をベースに、深さで少し調整して最短ルートを優先させる
            int CHECKMATE_SCORE = 999999000; // キングの価値より少し低く設定
            // 深さが残っているほど(depthが大きいほど)、より「早く」チェックメイトできることを意味する
            return isMaximizingPlayer ? (CHECKMATE_SCORE + depth) : -(CHECKMATE_SCORE + depth);
        }
        else
        {
            // ステールメイト
            return 0; // 引き分けは0点
        }
    }

    if (isMaximizingPlayer)
    {
        int maxEval = -2000000;
        for (const auto &move : moves)
        {
            Piece capturedPiece;
            makeMoveInternal(move, capturedPiece);
            // 評価関数の呼び出しにも alpha, beta を渡す
            int evaluation = minimax(depth - 1, false, alpha, beta);
            unmakeMoveInternal(move, capturedPiece);

            maxEval = std::max(maxEval, evaluation);
            alpha = std::max(alpha, maxEval); // ★ Alpha の更新

            if (beta <= alpha) // ★ Beta Cutoff (枝刈り)
            {
                break;
            }
        }
        return maxEval;
    }
    else // isMinimizingPlayer
    {
        int minEval = 2000000;
        for (const auto &move : moves)
        {
            Piece capturedPiece;
            makeMoveInternal(move, capturedPiece);
            // 評価関数の呼び出しにも alpha, beta を渡す
            int evaluation = minimax(depth - 1, true, alpha, beta);
            unmakeMoveInternal(move, capturedPiece);

            minEval = std::min(minEval, evaluation);
            beta = std::min(beta, minEval); // ★ Beta の更新

            if (beta <= alpha) // ★ Alpha Cutoff (枝刈り)
            {
                break;
            }
        }
        return minEval;
    }
}

Move ChessGame::bestMove(bool white)
{
    auto moves = generateMoves(white);
    if (moves.empty())
    {
        return {{0, 0}, {0, 0}};
    }

    int bestScore = white ? -20000000 : 20000000;
    Move best_move = moves[0];
    std::vector<Move> tiedMoves;

    for (const auto &move : moves)
    {
        Piece capturedPiece;
        makeMoveInternal(move, capturedPiece);

        const int INF = 25000000; // 評価関数の最大値より大きい値
        int score = minimax(MAX_DEPTH - 1, !white, -INF, INF);

        unmakeMoveInternal(move, capturedPiece);

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

Move ChessGame::ask(bool turnWhite)
{
    if (!turnWhite)
    {
        std::cout << "AI (Black) is thinking...\n";
        return bestMove(turnWhite);
    }

    std::cout << "Your turn (White). Enter move (e.g., e2e4 or e1g1 for Castling): ";
    std::string move_str;
    std::cin >> move_str;

    if (move_str.length() != 4)
    {
        std::cout << "Invalid format. Please use startSquareEndSquare (e.g., e2e4).\n";
        return ask(turnWhite);
    }
    int start_r, start_c, end_r, end_c;
    std::string start_alg = move_str.substr(0, 2);
    std::string end_alg = move_str.substr(2, 2);

    if (!algebraicToCoords(start_alg, start_r, start_c) || !algebraicToCoords(end_alg, end_r, end_c))
    {
        std::cout << "Invalid square coordinates.\n";
        return ask(turnWhite);
    }

    Move player_move = {{start_r, start_c}, {end_r, end_c}};
    auto valid_moves = generateMoves(turnWhite);

    bool is_valid = std::find(valid_moves.begin(), valid_moves.end(), player_move) != valid_moves.end();
    if (!is_valid)
    {
        std::cout << "Illegal move. Try again.\n";
        return ask(turnWhite);
    }
    if (board[start_r][start_c].type == '*' || board[start_r][start_c].isWhite != turnWhite)
    {
        std::cout << "No valid piece of your color on the starting square. Try again.\n";
        return ask(turnWhite);
    }

    return player_move;
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
        std::cout << (turnWhite ? "White" : "Black") << " moves: "
                  << char('a' + move.first.second) << 8 - move.first.first
                  << " -> "
                  << char('a' + move.second.second) << 8 - move.second.first
                  << "\n";

        makeMove(move);
        printBoard();
        turnWhite = !turnWhite;
    }

    std::cout << "\nGame finished (100 turns reached or Checkmate/Stalemate).\n";
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

std::string ChessGame::coordsToAlgebraic(int r, int c) const
{
    // c (0-7) -> 'a'-'h'
    char file = 'a' + c;

    // r (0-7) -> 8-1
    char rank = '8' - r; // 行インデックス 0 は 文字 '8' に、7 は '1' に変換される

    return std::string(1, file) + std::string(1, rank);
}

std::string ChessGame::moveToAlgebratic(Move move) const
{
    // Move.first.first = 開始行 (r1), Move.first.second = 開始列 (c1)
    int r1 = move.first.first;
    int c1 = move.first.second;

    // Move.second.first = 終了行 (r2), Move.second.second = 終了列 (c2)
    int r2 = move.second.first;
    int c2 = move.second.second;

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
    move.first = {startRow, startCol}; // 移動元 (行, 列)
    move.second = {endRow, endCol};    // 移動先 (行, 列)

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

bool ChessGame::isEnd(bool turnWhite)
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
        return true;
    }

    if (isDrawByThreefoldRepetition(turnWhite))
    {
        std::cout << "\n*** DRAW! Game is a DRAW by PERPETUAL CHECK. ***\n";

        return true;
    }

    bool opponentWhite = !turnWhite;
    if (!isKingOnBoard(opponentWhite))
    {
        // 相手（捕獲された側）のキングが消えた
        std::cout << "\n*** FATAL ERROR: KING CAPTURED! " << (turnWhite ? "White" : "Black") << " WINS! ***\n";
        std::cout << "NOTE: This should not happen if checkmate/check logic is perfect.\n";

        return true;
    }

    return false;
}

