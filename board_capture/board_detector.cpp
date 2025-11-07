#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <array>
#include <deque>
#include <chrono>
#include <cmath>
#include <sstream>

using namespace cv;
using namespace std;
using namespace chrono;


// JPEG警告を抑制
void suppressJPEGWarnings() {
    freopen("/dev/null", "w", stderr);
}

// 駒の種類
enum PieceType {
    EMPTY = 0,
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING
};

// プレイヤー
enum Player {
    WHITE = 0,
    BLACK = 1
};

// 盤面の状態
struct BoardState {
    array<PieceType, 64> pieces;
    steady_clock::time_point timestamp;
    
    bool operator==(const BoardState& other) const {
        return pieces == other.pieces;
    }
    
    int countPieces() const {
        int count = 0;
        for(auto p : pieces) if(p != EMPTY) count++;
        return count;
    }
};

// 着手情報
struct Move {
    int fromIdx;
    int toIdx;
    PieceType movedPiece;
    PieceType capturedPiece;
    
    string toString() const {
        auto idxToPos = [](int idx) {
            // x: 0=h, 7=a (右から左)
            // y: 0=rank1(上), 7=rank8(下)
            char file = 'h' - (idx % 8);
            int rank = (idx / 8) + 1;
            return string(1, file) + to_string(rank);
        };
        
        string from = idxToPos(fromIdx);
        string to = idxToPos(toIdx);
        
        return from + " -> " + to;
    }
};

// 色ベース検出器（detect_board_colorから移植）
class ColorBasedDetector {
private:
    Mat H;
    
    double colorVariance(const Mat& cellBGR) {
        Scalar meanColor = mean(cellBGR);
        Mat diff;
        absdiff(cellBGR, Scalar(meanColor[0], meanColor[1], meanColor[2]), diff);
        return mean(diff)[0] + mean(diff)[1] + mean(diff)[2];
    }
    
    pair<double, double> whiteBlackRatio(const Mat& cellBGR) {
        Mat gray;
        cvtColor(cellBGR, gray, COLOR_BGR2GRAY);
        
        int whiteCount = 0, blackCount = 0, totalPixels = gray.rows * gray.cols;
        
        for(int y = 0; y < gray.rows; y++) {
            for(int x = 0; x < gray.cols; x++) {
                uchar pixel = gray.at<uchar>(y, x);
                if(pixel > 180) whiteCount++;
                else if(pixel < 80) blackCount++;
            }
        }
        
        return {(double)whiteCount / totalPixels, (double)blackCount / totalPixels};
    }
    
    double edgeDensity(const Mat& cellBGR) {
        Mat gray;
        cvtColor(cellBGR, gray, COLOR_BGR2GRAY);
        Mat edges;
        Canny(gray, edges, 50, 150);
        return (double)countNonZero(edges) / (edges.rows * edges.cols);
    }
    
    struct CellFeatures {
        double variance;
        double whiteRatio;
        double blackRatio;
        double edgeDensity;
        double occupancyScore;
        bool hasWhitePiece;
        bool hasBlackPiece;
    };
    
    CellFeatures analyzCell(const Mat& boardBGR, int x, int y) {
        int cw = 100, ch = 100;
        Rect r(x*cw, y*ch, cw, ch);
        Mat cellBGR = boardBGR(r);
        
        CellFeatures features;
        features.variance = colorVariance(cellBGR);
        auto [whiteR, blackR] = whiteBlackRatio(cellBGR);
        features.whiteRatio = whiteR;
        features.blackRatio = blackR;
        features.edgeDensity = edgeDensity(cellBGR);
        
        features.occupancyScore = features.variance * 0.3 + 
                                  (features.whiteRatio + features.blackRatio) * 100.0 +
                                  features.edgeDensity * 50.0;
        
        // 白駒か黒駒かを判定（閾値を調整）
        features.hasWhitePiece = features.whiteRatio > 0.10 && features.whiteRatio > features.blackRatio * 1.3;
        features.hasBlackPiece = features.blackRatio > 0.10 && features.blackRatio > features.whiteRatio * 1.3;
        
        return features;
    }
    
    PieceType detectPieceType(int x, int y, bool isWhite) {
        int rank = y + 1; // y=0 -> rank 1, y=7 -> rank 8
        
        // 位置で判定（上2列=白(rank 1-2)、下2列=黒(rank 7-8)）
        bool isWhiteSide = (rank <= 2);
        
        if(isWhiteSide) {
            // 白駒エリア（上側、rank 1-2）
            if(rank == 2) return WHITE_PAWN;
            if(rank == 1) {
                if(x == 0 || x == 7) return WHITE_ROOK;
                if(x == 1 || x == 6) return WHITE_KNIGHT;
                if(x == 2 || x == 5) return WHITE_BISHOP;
                if(x == 3) return WHITE_QUEEN;
                if(x == 4) return WHITE_KING;
            }
            return WHITE_PAWN;
        } else {
            // 黒駒エリア（下側、rank 7-8）
            if(rank == 7) return BLACK_PAWN;
            if(rank == 8) {
                if(x == 0 || x == 7) return BLACK_ROOK;
                if(x == 1 || x == 6) return BLACK_KNIGHT;
                if(x == 2 || x == 5) return BLACK_BISHOP;
                if(x == 3) return BLACK_QUEEN;
                if(x == 4) return BLACK_KING;
            }
            return BLACK_PAWN;
        }
    }
    
public:
    bool loadHomography(const string& calibPath) {
        FileStorage fs(calibPath, FileStorage::READ);
        if(!fs.isOpened()) return false;
        
        fs["H"] >> H;
        fs.release();
        
        return !H.empty();
    }
    
    BoardState detectCurrentState(const Mat& frame, const BoardState& referenceState) {
        BoardState state;
        state.timestamp = steady_clock::now();
        
        Mat board;
        warpPerspective(frame, board, H, Size(800, 800));
        
        vector<pair<int, CellFeatures>> cellFeatures;
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 8; x++) {
                int idx = y * 8 + x;
                CellFeatures features = analyzCell(board, x, y);
                cellFeatures.push_back({idx, features});
            }
        }
        
        sort(cellFeatures.begin(), cellFeatures.end(),
             [](const auto& a, const auto& b) {
                 return a.second.occupancyScore > b.second.occupancyScore;
             });
        
        // 期待駒数に基づいて占有判定
        int expectedPieceCount = referenceState.countPieces();
        array<bool, 64> occupied;
        occupied.fill(false);
        
        if(expectedPieceCount > 0 && expectedPieceCount <= 32) {
            for(int i = 0; i < expectedPieceCount-1; i++) {
                int idx = cellFeatures[i].first;
                occupied[idx] = true;
            }
            if(cellFeatures[expectedPieceCount-2].second.occupancyScore - cellFeatures[expectedPieceCount-1].second.occupancyScore < 6.2) {
                int idx = cellFeatures[expectedPieceCount-1].first;
                occupied[idx] = true;
            }
        } else {
            for(const auto& [idx, features] : cellFeatures) {
                if(features.occupancyScore > 30.0) {
                    occupied[idx] = true;
                }
            }
        }
        
        // 占有情報のみ更新、駒の種類は参照盤面から完全にコピー
        state.pieces = referenceState.pieces;
        
        // 消えた駒と現れた場所を検出
        vector<int> disappeared, appeared;
        for(int i = 0; i < 64; i++) {
            bool wasOccupied = (referenceState.pieces[i] != EMPTY);
            bool isOccupied = occupied[i];
            
            if(wasOccupied && !isOccupied) {
                // 駒が消えた
                state.pieces[i] = EMPTY;
                disappeared.push_back(i);
            } else if(!wasOccupied && isOccupied) {
                // 駒が現れた
                appeared.push_back(i);
            }
        }
        
        // 1つ消えて1つ現れた場合、駒を移動
        if(disappeared.size() == 1 && appeared.size() == 1) {
            state.pieces[appeared[0]] = referenceState.pieces[disappeared[0]];
        }
        else if(disappeared.size() == 1&& appeared.size() == 0) {
            int disappearedIdx = disappeared[0];
            state.pieces[disappearedIdx] = EMPTY;
             // 画像から色判定を試みる
            
            PieceType movedPiece = referenceState.pieces[disappearedIdx];
            
            // 色判定が明確な場合
            if(movedPiece >= WHITE_PAWN && movedPiece <= WHITE_KING) {
                // 白駒が現れた
                for(int idx = 0; idx < 64; idx++) {
                    if(occupied[idx]){
                        PieceType oldPiece = state.pieces[idx];
                        if(oldPiece >= BLACK_PAWN && oldPiece <= BLACK_KING) {
                            CellFeatures features = analyzCell(board, idx % 8, idx / 8);
                            if(features.hasWhitePiece) {
                                state.pieces[idx] = movedPiece;
                                break;
                            }
                        }
                    }
                }
            } else if(movedPiece >= BLACK_PAWN && movedPiece <= BLACK_KING) {
                // 黒駒が現れた
                for(int idx = 0; idx < 64; idx++) {
                    if(occupied[idx]){
                        PieceType oldPiece = state.pieces[idx];
                        if(oldPiece >= WHITE_PAWN && oldPiece <= WHITE_KING) {
                            CellFeatures features = analyzCell(board, idx % 8, idx / 8);
                            if(features.hasBlackPiece) {
                                state.pieces[idx] = movedPiece;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        return state;
    }
};

// チェスルール検証器
class ChessValidator {
private:
    Player getPlayer(PieceType p) {
        if(p >= WHITE_PAWN && p <= WHITE_KING) return WHITE;
        if(p >= BLACK_PAWN && p <= BLACK_KING) return BLACK;
        return WHITE; // EMPTY
    }
    
    bool isValidPawnMove(int from, int to, PieceType piece, PieceType captured) {
        int fromX = from % 8, fromY = from / 8;
        int toX = to % 8, toY = to / 8;
        int dx = toX - fromX;
        int dy = toY - fromY;
        
        if(piece == WHITE_PAWN) {
            // 白ポーンは下に進む（y増加、rank増加）
            if(dy == 1 && dx == 0 && captured == EMPTY) return true; // 1マス前進
            if(dy == 2 && dx == 0 && fromY == 1 && captured == EMPTY) return true; // rank2から2マス
            if(dy == 1 && abs(dx) == 1 && captured != EMPTY) return true; // 斜め取り
        } else if(piece == BLACK_PAWN) {
            // 黒ポーンは上に進む（y減少、rank減少）
            if(dy == -1 && dx == 0 && captured == EMPTY) return true;
            if(dy == -2 && dx == 0 && fromY == 6 && captured == EMPTY) return true; // rank7から2マス
            if(dy == -1 && abs(dx) == 1 && captured != EMPTY) return true;
        }
        return false;
    }
    
    bool isValidKnightMove(int from, int to) {
        int fromX = from % 8, fromY = from / 8;
        int toX = to % 8, toY = to / 8;
        int dx = abs(toX - fromX);
        int dy = abs(toY - fromY);
        return (dx == 2 && dy == 1) || (dx == 1 && dy == 2);
    }
    
    bool isValidBishopMove(int from, int to) {
        int fromX = from % 8, fromY = from / 8;
        int toX = to % 8, toY = to / 8;
        return abs(toX - fromX) == abs(toY - fromY);
    }
    
    bool isValidRookMove(int from, int to) {
        int fromX = from % 8, fromY = from / 8;
        int toX = to % 8, toY = to / 8;
        return (fromX == toX) || (fromY == toY);
    }
    
    bool isValidQueenMove(int from, int to) {
        return isValidBishopMove(from, to) || isValidRookMove(from, to);
    }
    
    bool isValidKingMove(int from, int to) {
        int fromX = from % 8, fromY = from / 8;
        int toX = to % 8, toY = to / 8;
        int dx = abs(toX - fromX);
        int dy = abs(toY - fromY);
        return dx <= 1 && dy <= 1;
    }
    
public:
    bool isValidMove(const Move& move, Player currentPlayer) {
        // プレイヤーチェック
        Player pieceOwner = getPlayer(move.movedPiece);
        if(pieceOwner != currentPlayer) {
            return false;
        }
        
        // 自分の駒を取ることはできない
        if(move.capturedPiece != EMPTY && getPlayer(move.capturedPiece) == currentPlayer) {
            return false;
        }
        
        // 駒の種類ごとのルール
        PieceType baseType = move.movedPiece;
        if(baseType == WHITE_PAWN || baseType == BLACK_PAWN) {
            return isValidPawnMove(move.fromIdx, move.toIdx, baseType, move.capturedPiece);
        } else if(baseType == WHITE_KNIGHT || baseType == BLACK_KNIGHT) {
            return isValidKnightMove(move.fromIdx, move.toIdx);
        } else if(baseType == WHITE_BISHOP || baseType == BLACK_BISHOP) {
            return isValidBishopMove(move.fromIdx, move.toIdx);
        } else if(baseType == WHITE_ROOK || baseType == BLACK_ROOK) {
            return isValidRookMove(move.fromIdx, move.toIdx);
        } else if(baseType == WHITE_QUEEN || baseType == BLACK_QUEEN) {
            return isValidQueenMove(move.fromIdx, move.toIdx);
        } else if(baseType == WHITE_KING || baseType == BLACK_KING) {
            return isValidKingMove(move.fromIdx, move.toIdx);
        }
        
        return false;
    }
};

// チェス検出システム
class ChessDetectionSystem {
private:
    ColorBasedDetector detector;
    ChessValidator validator;
    BoardState lastConfirmedState;
    Player currentPlayer = WHITE;
    deque<BoardState> stateHistory;
    const double CONFIRM_INTERVAL = 5.0;
    
    static string pieceToString(PieceType p) {
        switch(p) {
            case EMPTY: return ".";
            case WHITE_PAWN: return "P";
            case WHITE_KNIGHT: return "N";
            case WHITE_BISHOP: return "B";
            case WHITE_ROOK: return "R";
            case WHITE_QUEEN: return "Q";
            case WHITE_KING: return "K";
            case BLACK_PAWN: return "p";
            case BLACK_KNIGHT: return "n";
            case BLACK_BISHOP: return "b";
            case BLACK_ROOK: return "r";
            case BLACK_QUEEN: return "q";
            case BLACK_KING: return "k";
            default: return "?";
        }
    }
    
    Player getPlayerFromPiece(PieceType p) {
        if(p >= WHITE_PAWN && p <= WHITE_KING) return WHITE;
        if(p >= BLACK_PAWN && p <= BLACK_KING) return BLACK;
        return WHITE;
    }
    
    vector<Move> detectMoves(const BoardState& oldState, const BoardState& newState) {
        vector<Move> moves;
        
        // 消えた駒と現れた駒を検出（位置のみ、色は無視）
        vector<int> disappeared, appeared;
        
        for(int i = 0; i < 64; i++) {
            if(oldState.pieces[i] != EMPTY && newState.pieces[i] == EMPTY) {
                disappeared.push_back(i);
            }
            if(oldState.pieces[i] == EMPTY && newState.pieces[i] != EMPTY) {
                appeared.push_back(i);
            }
        }
        
        // 通常の移動：1つ消えて1つ現れる
        if(disappeared.size() == 1 && appeared.size() == 1) {
            Move move;
            move.fromIdx = disappeared[0];
            move.toIdx = appeared[0];
            // 前の盤面から駒の種類を取得（色も維持）
            move.movedPiece = oldState.pieces[disappeared[0]];
            move.capturedPiece = EMPTY;
            moves.push_back(move);
        }
        else if(disappeared.size() == 1 && appeared.size() == 0) {
            Move move;
            // どちらが移動元か判定（現在のプレイヤーの駒の方が移動元）
            int fromIdx = -1, captureIdx = -1;
            Player movingPlayer = currentPlayer;
            
            int idx = disappeared[0];
            PieceType piece = oldState.pieces[idx];
            Player pieceOwner = getPlayerFromPiece(piece);
            if(pieceOwner == movingPlayer) {
                fromIdx = idx;
            }
            for(int i = 0; i < 64; i++) {
                if(oldState.pieces[i] != newState.pieces[i] && i != idx) {
                    captureIdx = i;
                    break;
                }
            }
            
            if(fromIdx != -1 && captureIdx != -1) {
                move.fromIdx = fromIdx;
                move.toIdx = captureIdx;
                move.movedPiece = oldState.pieces[fromIdx];
                move.capturedPiece = oldState.pieces[captureIdx];
                moves.push_back(move);
            }
        }
        
        return moves;
    }
    
public:
    bool initialize(const string& calibPath) {
        if(!detector.loadHomography(calibPath)) {
            return false;
        }
        
        // 初期状態設定
        // y=0がrank1(上側、白)、y=7がrank8(下側、黒)
        lastConfirmedState.pieces.fill(EMPTY);
        
        // 白の駒（上側、rank 1-2）
        lastConfirmedState.pieces[0] = WHITE_ROOK;
        lastConfirmedState.pieces[1] = WHITE_KNIGHT;
        lastConfirmedState.pieces[2] = WHITE_BISHOP;
        lastConfirmedState.pieces[3] = WHITE_QUEEN;
        lastConfirmedState.pieces[4] = WHITE_KING;
        lastConfirmedState.pieces[5] = WHITE_BISHOP;
        lastConfirmedState.pieces[6] = WHITE_KNIGHT;
        lastConfirmedState.pieces[7] = WHITE_ROOK;
        for(int i = 8; i < 16; i++) lastConfirmedState.pieces[i] = WHITE_PAWN;
        
        // 黒の駒（下側、rank 7-8）
        for(int i = 48; i < 56; i++) lastConfirmedState.pieces[i] = BLACK_PAWN;
        lastConfirmedState.pieces[56] = BLACK_ROOK;
        lastConfirmedState.pieces[57] = BLACK_KNIGHT;
        lastConfirmedState.pieces[58] = BLACK_BISHOP;
        lastConfirmedState.pieces[59] = BLACK_QUEEN;
        lastConfirmedState.pieces[60] = BLACK_KING;
        lastConfirmedState.pieces[61] = BLACK_BISHOP;
        lastConfirmedState.pieces[62] = BLACK_KNIGHT;
        lastConfirmedState.pieces[63] = BLACK_ROOK;
        
        lastConfirmedState.timestamp = steady_clock::now();
        
        return true;
    }
    
    void setInitialState(const BoardState& state) {
        lastConfirmedState = state;
        stateHistory.clear();
        currentPlayer = WHITE;
    }
    
    BoardState detectCurrentState(const Mat& frame) {
        return detector.detectCurrentState(frame, lastConfirmedState);
    }
    
    pair<bool, BoardState> tryConfirmState(const Mat& frame) {
        BoardState current = detectCurrentState(frame);
        
        // 駒取りの場合の駒種類を確定
        vector<int> disappeared, appeared;
        for(int i = 0; i < 64; i++) {
            if(lastConfirmedState.pieces[i] != EMPTY && current.pieces[i] == EMPTY) {
                disappeared.push_back(i);
            }
            if(lastConfirmedState.pieces[i] == EMPTY && current.pieces[i] != EMPTY) {
                appeared.push_back(i);
            }
        }
        
        // detectCurrentStateで既に駒取りの処理は完了しているので、ここでは何もしない
        
        auto now = steady_clock::now();
        
        // 履歴が空、または最新の状態と異なる場合はリセット
        if(stateHistory.empty() || !(stateHistory.back() == current)) {
            stateHistory.clear();
            stateHistory.push_back(current);
            return {false, BoardState()};
        }
        
        // 同じ状態が続いている場合、経過時間をチェック
        auto stableTime = duration_cast<duration<double>>(now - stateHistory.front().timestamp).count();
        
        if(stableTime >= CONFIRM_INTERVAL) {
            // 5秒間安定 → 確定
            BoardState confirmedState = stateHistory.front();
            
            // 前回の確定状態と異なる場合のみ返す
            if(!(confirmedState == lastConfirmedState)) {
                return {true, confirmedState};
            } else {
                // 同じ状態 → 履歴をクリアして次を待つ
                stateHistory.clear();
                stateHistory.push_back(current);
                return {false, BoardState()};
            }
        }
        
        return {false, BoardState()};
    }
    
    void processConfirmedState(const BoardState& newState) {
        cout << "\n\033[1;36m=== Board State Confirmed ===\033[0m" << endl;
        
        // 着手検出
        vector<Move> moves = detectMoves(lastConfirmedState, newState);
        
        if(moves.empty()) {
            cout << "\033[1;33mNo move detected\033[0m" << endl;
        } else {
            for(const auto& move : moves) {
                cout << "\nMove detected: " << pieceToString(move.movedPiece) << " " << move.toString();
                if(move.capturedPiece != EMPTY) {
                    cout << " (captured " << pieceToString(move.capturedPiece) << ")";
                }
                cout << endl;
                
                // デバッグ情報
                int fromX = move.fromIdx % 8, fromY = move.fromIdx / 8;
                int toX = move.toIdx % 8, toY = move.toIdx / 8;
                cout << "  Debug: from(" << fromX << "," << fromY << ") to(" << toX << "," << toY << ")" << endl;
                cout << "  Current player: " << (currentPlayer == WHITE ? "WHITE" : "BLACK") << endl;
                cout << "  Piece type: " << (int)move.movedPiece << " (WHITE=1-6, BLACK=7-12)" << endl;
                
                // ルール検証
                bool valid = validator.isValidMove(move, currentPlayer);
                
                if(valid) {
                    cout << "\033[1;32m✓ Valid move for " << (currentPlayer == WHITE ? "WHITE" : "BLACK") << "\033[0m" << endl;
                    
                    // 手番交代
                    currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;
                    lastConfirmedState = newState;
                } else {
                    cout << "\033[1;31m✗ ILLEGAL MOVE!\033[0m" << endl;
                    cout << "  Reason: Invalid move for " << (currentPlayer == WHITE ? "WHITE" : "BLACK") << endl;
                    // 盤面を更新しない（元に戻すべき）
                    return;
                }
            }
        }
        
        // 盤面表示
        printBoard(newState);
        cout << "\nNext turn: \033[1m" << (currentPlayer == WHITE ? "WHITE" : "BLACK") << "\033[0m" << endl;
    }
    
    void printBoard(const BoardState& state) {
        cout << "\n  h g f e d c b a" << endl;
        for(int y = 0; y < 8; y++) {
            int rank = y + 1;
            cout << rank << " ";
            for(int x = 0; x < 8; x++) {
                int idx = y * 8 + x;
                cout << pieceToString(state.pieces[idx]) << " ";
            }
            cout << rank << endl;
        }
        cout << "  h g f e d c b a" << endl;
    }
    
    BoardState getLastConfirmedState() const {
        return lastConfirmedState;
    }
    
    int getStableTime() const {
        if(stateHistory.empty()) return 0;
        auto now = steady_clock::now();
        auto age = duration_cast<duration<double>>(now - stateHistory.front().timestamp).count();
        return (int)age;
    }
    
    Mat visualizeBoard(const Mat& frame, const BoardState& state) {
        Mat board;
        Mat H;
        
        // ホモグラフィを取得
        FileStorage fs("calib.yaml", FileStorage::READ);
        fs["H"] >> H;
        fs.release();
        
        warpPerspective(frame, board, H, Size(800, 800));
        
        // 各マスに枠を描画
        int cw = 100, ch = 100;
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 8; x++) {
                int idx = y * 8 + x;
                Rect r(x*cw, y*ch, cw, ch);
                PieceType p = state.pieces[idx];
                
                Scalar col;
                if(p == EMPTY) {
                    col = Scalar(0, 255, 0); // 緑：空
                } else if(p >= WHITE_PAWN && p <= WHITE_KING) {
                    col = Scalar(255, 255, 0); // シアン：白駒
                } else {
                    col = Scalar(0, 0, 255); // 赤：黒駒
                }
                rectangle(board, r, col, 2);
            }
        }
        
        // 情報表示
        int pieceCount = state.countPieces();
        putText(board, "Pieces: " + to_string(pieceCount), {10, 30},
                FONT_HERSHEY_SIMPLEX, 0.8, Scalar(255, 255, 255), 2);
        
        putText(board, "Next: " + string(currentPlayer == WHITE ? "WHITE" : "BLACK"), {10, 60},
                FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255, 255, 0), 2);
        
        // 安定時間を表示
        int stableTime = getStableTime();
        if(stableTime > 0) {
            string statusText = "Stable: " + to_string(stableTime) + " / 5s";
            putText(board, statusText, {10, 780},
                    FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 255), 2);
        }
        
        return board;
    }
};

int main(int argc, char** argv) {
    suppressJPEGWarnings();
    
    string calibPath = "calib.yaml";
    int cam = 0;
    string devPath = "";
    
    for(int i = 1; i < argc; i++) {
        string a = argv[i];
        if(a == "--camera" && i+1 < argc) cam = atoi(argv[++i]);
        else if(a == "--device" && i+1 < argc) devPath = argv[++i];
        else if(a == "--calib" && i+1 < argc) calibPath = argv[++i];
    }
    
    ChessDetectionSystem system;
    if(!system.initialize(calibPath)) {
        cerr << "Failed to initialize system" << endl;
        return 1;
    }
    
    cout << "Chess Detection System initialized" << endl;
    
    VideoCapture cap;
    if(!devPath.empty()) cap.open(devPath, CAP_V4L2);
    else cap.open(cam, CAP_V4L2);
    
    if(!cap.isOpened()) {
        cerr << "Camera open failed" << endl;
        return 1;
    }
    
    cap.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M','J','P','G'));
    cap.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap.set(CAP_PROP_FRAME_HEIGHT, 1080);
    cap.set(CAP_PROP_FPS, 30);
    
    // 初期位置設定
    cout << "\n=== Initial Position Setup ===" << endl;
    cout << "Detecting initial board position..." << endl;
    cout << "Press 'SPACE' to confirm, or 'ESC' to use default" << endl;
    
    bool initialSet = false;
    auto lastPrint = steady_clock::now();
    
    while(!initialSet) {
        Mat frame;
        cap >> frame;
        if(frame.empty()) break;
        
        BoardState currentState = system.detectCurrentState(frame);
        
        // 盤面を可視化
        Mat display = system.visualizeBoard(frame, currentState);
        
        // 1秒ごとに現在の盤面を表示
        auto now = steady_clock::now();
        auto elapsed = duration_cast<duration<double>>(now - lastPrint).count();
        if(elapsed >= 1.0) {
            cout << "\n--- Current Detection (Pieces: " << currentState.countPieces() << ") ---" << endl;
            system.printBoard(currentState);
            cout << "Press SPACE to confirm this position" << endl;
            lastPrint = now;
        }
        
        imshow("Chess Detection", display);
        
        int key = waitKey(1);
        if(key == 27) {
            cout << "\nUsing default initial position" << endl;
            initialSet = true;
        } else if(key == ' ' || key == 32) {
            system.setInitialState(currentState);
            cout << "\n✓ Initial position confirmed!" << endl;
            system.printBoard(currentState);
            initialSet = true;
        }
    }
    
    cout << "\n\033[1;32m=== Starting Chess Game Detection ===\033[0m" << endl;
    cout << "Detecting board every 0.5 seconds..." << endl;
    cout << "Board confirmed after 5 seconds of stability" << endl;
    cout << "Next turn: WHITE" << endl;
    
    auto clearScreen = []() {
        cout << "\033[2J\033[1;1H";
    };
    
    auto lastStatusPrint = steady_clock::now();
    auto lastDetectionTime = steady_clock::now();
    const double DETECTION_INTERVAL = 0.5; // 0.5秒ごとに検知
    
    while(true) {
        Mat frame;
        cap >> frame;
        if(frame.empty()) break;
        
        auto now = steady_clock::now();
        auto detectionElapsed = duration_cast<duration<double>>(now - lastDetectionTime).count();
        
        // 0.5秒ごとに盤面を検知
        if(detectionElapsed >= DETECTION_INTERVAL) {
            auto [confirmed, state] = system.tryConfirmState(frame);
            
            if(confirmed) {
                clearScreen();
                system.processConfirmedState(state);
            }
            
            lastDetectionTime = now;
        }
        
        // 1秒ごとに進捗状況を表示
        auto statusElapsed = duration_cast<duration<double>>(now - lastStatusPrint).count();
        if(statusElapsed >= 1.0) {
            int stableTime = system.getStableTime();
            if(stableTime > 0) {
                cout << "\rWaiting for stable board: " << stableTime << " / 5 seconds..." << flush;
            } else {
                cout << "\rWaiting for move...                    " << flush;
            }
            lastStatusPrint = now;
        }
        
        // 現在の検出状態を可視化
        BoardState currentState = system.detectCurrentState(frame);
        Mat display = system.visualizeBoard(frame, currentState);
        
        imshow("Chess Detection", display);
        
        int key = waitKey(1);
        if(key == 27) break;
    }
    
    cout << "\nGame ended." << endl;
    return 0;
}
