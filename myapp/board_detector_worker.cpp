#include "board_detector_worker.hpp"
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

// ==== あなたの単体版コードをフル移植（必要最小の形で） ====

// JPEG警告を抑制
#include <cstdio>
static void suppressJPEGWarnings() {
    FILE* f = freopen("/dev/null", "w", stderr);
    (void)f;
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
        for (auto p : pieces) if (p != EMPTY) count++;
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

// 色ベース検出器
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
        for (int y = 0; y < gray.rows; y++) {
            for (int x = 0; x < gray.cols; x++) {
                uchar pixel = gray.at<uchar>(y, x);
                if (pixel > 180) whiteCount++;
                else if (pixel < 80) blackCount++;
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
        Rect r(x * cw, y * ch, cw, ch);
        Mat cellBGR = boardBGR(r);

        CellFeatures f;
        f.variance = colorVariance(cellBGR);
        auto [whiteR, blackR] = whiteBlackRatio(cellBGR);
        f.whiteRatio = whiteR;
        f.blackRatio = blackR;
        f.edgeDensity = edgeDensity(cellBGR);
        f.occupancyScore = f.variance * 0.3 +
                           (f.whiteRatio + f.blackRatio) * 100.0 +
                           f.edgeDensity * 50.0;
        f.hasWhitePiece = f.whiteRatio > 0.10 && f.whiteRatio > f.blackRatio * 1.3;
        f.hasBlackPiece = f.blackRatio > 0.10 && f.blackRatio > f.whiteRatio * 1.3;
        return f;
    }

public:
    bool loadHomography(const string& calibPath) {
        FileStorage fs(calibPath, FileStorage::READ);
        if (!fs.isOpened()) return false;
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
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                int idx = y * 8 + x;
                auto cf = analyzCell(board, x, y);
                cellFeatures.push_back({idx, cf});
            }
        }

        sort(cellFeatures.begin(), cellFeatures.end(),
             [](const auto& a, const auto& b) {
                 return a.second.occupancyScore > b.second.occupancyScore;
             });

        int expectedPieceCount = referenceState.countPieces();
        array<bool, 64> occupied;
        occupied.fill(false);

        if (expectedPieceCount > 0 && expectedPieceCount <= 32) {
            for (int i = 0; i < expectedPieceCount - 1; i++) {
                int idx = cellFeatures[i].first;
                occupied[idx] = true;
            }
            if (cellFeatures[expectedPieceCount - 2].second.occupancyScore -
                cellFeatures[expectedPieceCount - 1].second.occupancyScore < 6.2) {
                int idx = cellFeatures[expectedPieceCount - 1].first;
                occupied[idx] = true;
            }
        } else {
            for (const auto& [idx, f] : cellFeatures) {
                if (f.occupancyScore > 30.0) {
                    occupied[idx] = true;
                }
            }
        }

        state.pieces = referenceState.pieces;

        vector<int> disappeared, appeared;
        for (int i = 0; i < 64; i++) {
            bool was = (referenceState.pieces[i] != EMPTY);
            bool is = occupied[i];
            if (was && !is) {
                state.pieces[i] = EMPTY;
                disappeared.push_back(i);
            } else if (!was && is) {
                appeared.push_back(i);
            }
        }

        if (disappeared.size() == 1 && appeared.size() == 1) {
            state.pieces[appeared[0]] = referenceState.pieces[disappeared[0]];
        } else if (disappeared.size() == 1 && appeared.size() == 0) {
            int disappearedIdx = disappeared[0];
            state.pieces[disappearedIdx] = EMPTY;
            PieceType movedPiece = referenceState.pieces[disappearedIdx];

            if (movedPiece >= WHITE_PAWN && movedPiece <= WHITE_KING) {
                for (int idx = 0; idx < 64; idx++) {
                    if (occupied[idx]) {
                        PieceType oldPiece = state.pieces[idx];
                        if (oldPiece >= BLACK_PAWN && oldPiece <= BLACK_KING) {
                            auto f = analyzCell(board, idx % 8, idx / 8);
                            if (f.hasWhitePiece) {
                                state.pieces[idx] = movedPiece;
                                break;
                            }
                        }
                    }
                }
            } else if (movedPiece >= BLACK_PAWN && movedPiece <= BLACK_KING) {
                for (int idx = 0; idx < 64; idx++) {
                    if (occupied[idx]) {
                        PieceType oldPiece = state.pieces[idx];
                        if (oldPiece >= WHITE_PAWN && oldPiece <= WHITE_KING) {
                            auto f = analyzCell(board, idx % 8, idx / 8);
                            if (f.hasBlackPiece) {
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
        if (p >= WHITE_PAWN && p <= WHITE_KING) return WHITE;
        if (p >= BLACK_PAWN && p <= BLACK_KING) return BLACK;
        return WHITE;
    }

    bool isValidPawnMove(int from, int to, PieceType piece, PieceType captured) {
        int fx = from % 8, fy = from / 8;
        int tx = to % 8, ty = to / 8;
        int dx = tx - fx;
        int dy = ty - fy;

        if (piece == WHITE_PAWN) {
            if (dy == 1 && dx == 0 && captured == EMPTY) return true;
            if (dy == 2 && dx == 0 && fy == 1 && captured == EMPTY) return true;
            if (dy == 1 && abs(dx) == 1 && captured != EMPTY) return true;
        } else if (piece == BLACK_PAWN) {
            if (dy == -1 && dx == 0 && captured == EMPTY) return true;
            if (dy == -2 && dx == 0 && fy == 6 && captured == EMPTY) return true;
            if (dy == -1 && abs(dx) == 1 && captured != EMPTY) return true;
        }
        return false;
    }

    bool isValidKnightMove(int from, int to) {
        int fx = from % 8, fy = from / 8;
        int tx = to % 8, ty = to / 8;
        int dx = abs(tx - fx);
        int dy = abs(ty - fy);
        return (dx == 2 && dy == 1) || (dx == 1 && dy == 2);
    }

    bool isValidBishopMove(int from, int to) {
        int fx = from % 8, fy = from / 8;
        int tx = to % 8, ty = to / 8;
        return abs(tx - fx) == abs(ty - fy);
    }

    bool isValidRookMove(int from, int to) {
        int fx = from % 8, fy = from / 8;
        int tx = to % 8, ty = to / 8;
        return (fx == tx) || (fy == ty);
    }

    bool isValidQueenMove(int from, int to) {
        return isValidBishopMove(from, to) || isValidRookMove(from, to);
    }

    bool isValidKingMove(int from, int to) {
        int fx = from % 8, fy = from / 8;
        int tx = to % 8, ty = to / 8;
        int dx = abs(tx - fx);
        int dy = abs(ty - fy);
        return dx <= 1 && dy <= 1;
    }

public:
    bool isValidMove(const Move& move, Player currentPlayer) {
        Player owner = getPlayer(move.movedPiece);
        if (owner != currentPlayer) return false;
        if (move.capturedPiece != EMPTY && getPlayer(move.capturedPiece) == currentPlayer) return false;

        PieceType p = move.movedPiece;
        if (p == WHITE_PAWN || p == BLACK_PAWN) return isValidPawnMove(move.fromIdx, move.toIdx, p, move.capturedPiece);
        if (p == WHITE_KNIGHT || p == BLACK_KNIGHT) return isValidKnightMove(move.fromIdx, move.toIdx);
        if (p == WHITE_BISHOP || p == BLACK_BISHOP) return isValidBishopMove(move.fromIdx, move.toIdx);
        if (p == WHITE_ROOK || p == BLACK_ROOK) return isValidRookMove(move.fromIdx, move.toIdx);
        if (p == WHITE_QUEEN || p == BLACK_QUEEN) return isValidQueenMove(move.fromIdx, move.toIdx);
        if (p == WHITE_KING || p == BLACK_KING) return isValidKingMove(move.fromIdx, move.toIdx);
        return false;
    }
};

// チェス検出システム（あなたの main 相当のロジックをまとめたクラス）
class ChessDetectionSystem {
private:
    ColorBasedDetector detector;
    ChessValidator validator;
    BoardState lastConfirmedState;
    Player currentPlayer = WHITE;
    deque<BoardState> stateHistory;
    const double CONFIRM_INTERVAL = 5.0;

    static string pieceToString(PieceType p) {
        switch (p) {
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
        if (p >= WHITE_PAWN && p <= WHITE_KING) return WHITE;
        if (p >= BLACK_PAWN && p <= BLACK_KING) return BLACK;
        return WHITE;
    }

    vector<Move> detectMoves(const BoardState& oldState, const BoardState& newState) {
        vector<Move> moves;
        vector<int> disappeared, appeared;

        for (int i = 0; i < 64; i++) {
            if (oldState.pieces[i] != EMPTY && newState.pieces[i] == EMPTY) disappeared.push_back(i);
            if (oldState.pieces[i] == EMPTY && newState.pieces[i] != EMPTY) appeared.push_back(i);
        }

        if (disappeared.size() == 1 && appeared.size() == 1) {
            Move m;
            m.fromIdx = disappeared[0];
            m.toIdx = appeared[0];
            m.movedPiece = oldState.pieces[disappeared[0]];
            m.capturedPiece = EMPTY;
            moves.push_back(m);
        } else if (disappeared.size() == 1 && appeared.size() == 0) {
            Move m;
            int fromIdx = -1, captureIdx = -1;
            Player movingPlayer = currentPlayer;

            int idx = disappeared[0];
            PieceType piece = oldState.pieces[idx];
            Player owner = getPlayerFromPiece(piece);
            if (owner == movingPlayer) fromIdx = idx;

            for (int i = 0; i < 64; i++) {
                if (oldState.pieces[i] != newState.pieces[i] && i != idx) {
                    captureIdx = i;
                    break;
                }
            }

            if (fromIdx != -1 && captureIdx != -1) {
                m.fromIdx = fromIdx;
                m.toIdx = captureIdx;
                m.movedPiece = oldState.pieces[fromIdx];
                m.capturedPiece = oldState.pieces[captureIdx];
                moves.push_back(m);
            }
        }
        return moves;
    }

public:
    bool initialize(const string& calibPath) {
        if (!detector.loadHomography(calibPath)) {
            return false;
        }
        lastConfirmedState.pieces.fill(EMPTY);

        // 初期配置（あなたの元コード通り）
        lastConfirmedState.pieces[0] = WHITE_ROOK;
        lastConfirmedState.pieces[1] = WHITE_KNIGHT;
        lastConfirmedState.pieces[2] = WHITE_BISHOP;
        lastConfirmedState.pieces[3] = WHITE_QUEEN;
        lastConfirmedState.pieces[4] = WHITE_KING;
        lastConfirmedState.pieces[5] = WHITE_BISHOP;
        lastConfirmedState.pieces[6] = WHITE_KNIGHT;
        lastConfirmedState.pieces[7] = WHITE_ROOK;
        for (int i = 8; i < 16; i++) lastConfirmedState.pieces[i] = WHITE_PAWN;

        for (int i = 48; i < 56; i++) lastConfirmedState.pieces[i] = BLACK_PAWN;
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

        // detectCurrentStateで駒取り処理は済みの前提

        auto now = steady_clock::now();

        if (stateHistory.empty() || !(stateHistory.back() == current)) {
            stateHistory.clear();
            stateHistory.push_back(current);
            return {false, BoardState()};
        }

        auto stableTime = duration_cast<duration<double>>(now - stateHistory.front().timestamp).count();

        if (stableTime >= 5.0) { // CONFIRM_INTERVAL
            BoardState confirmedState = stateHistory.front();
            if (!(confirmedState == lastConfirmedState)) {
                return {true, confirmedState};
            } else {
                stateHistory.clear();
                stateHistory.push_back(current);
                return {false, BoardState()};
            }
        }
        return {false, BoardState()};
    }

    void processConfirmedState(const BoardState& newState) {
        // 最小限の状態遷移（元コードのログは省略せず移すことも可）
        vector<Move> moves = detectMoves(lastConfirmedState, newState);
        if (!moves.empty()) {
            const auto& m = moves.back();
            // 検証（最低限）
            bool valid = validator.isValidMove(m, currentPlayer);
            if (valid) {
                currentPlayer = (currentPlayer == WHITE) ? BLACK : WHITE;
                lastConfirmedState = newState;
            } else {
                // 不正なら状態を進めない
                return;
            }
        } else {
            // 手が特定できない場合は盤だけ更新しない
            return;
        }
    }

    void printBoard(const BoardState& state) {
        cout << "\n  h g f e d c b a" << endl;
        for (int y = 0; y < 8; y++) {
            int rank = y + 1;
            cout << rank << " ";
            for (int x = 0; x < 8; x++) {
                int idx = y * 8 + x;
                cout << (state.pieces[idx] == EMPTY ? "." : "?") << " ";
            }
            cout << rank << endl;
        }
        cout << "  h g f e d c b a" << endl;
    }

    BoardState getLastConfirmedState() const {
        return lastConfirmedState;
    }

    int getStableTime() const {
        if (stateHistory.empty()) return 0;
        auto now = steady_clock::now();
        auto age = duration_cast<duration<double>>(now - stateHistory.front().timestamp).count();
        return (int)age;
    }
};

// ==== BoardDetectorWorker 実装 ====

BoardDetectorWorker::BoardDetectorWorker(QString calibPath, QString device, int camIndex, QObject* parent)
    : QObject(parent),
      calibPath_(std::move(calibPath)),
      device_(std::move(device)),
      camIndex_(camIndex),
      system_(std::make_unique<ChessDetectionSystem>()) 
      {}

BoardDetectorWorker::~BoardDetectorWorker() = default;

void BoardDetectorWorker::run() {
    if (!initSystem()) { emit error("Failed to initialize ChessDetectionSystem"); return; }
    if (!openCamera()) { emit error("Failed to open camera"); return; }
    emit status("Board detector started");

    lastDetect_ = steady_clock::now();

    while (running_.loadAcquire()) {
        bool whiteMoved = false;
        QString text;
        if (stepOnce(whiteMoved, text)) {
            if (whiteMoved) {
                emit boardForWhite(text); // ★ 本命通知
            }
        }
        QThread::msleep(1);
    }

    if (cap_.isOpened()) cap_.release();
    emit status("Board detector stopped");
}

void BoardDetectorWorker::stop() {
    running_.storeRelease(0);
}

bool BoardDetectorWorker::initSystem() {
    suppressJPEGWarnings();
    return system_->initialize(calibPath_.toStdString());
}

bool BoardDetectorWorker::openCamera() {
    if (!device_.isEmpty()) cap_.open(device_.toStdString(), CAP_V4L2);
    else cap_.open(camIndex_, CAP_V4L2);
    if (!cap_.isOpened()) return false;

    cap_.set(CAP_PROP_FOURCC, VideoWriter::fourcc('M','J','P','G'));
    cap_.set(CAP_PROP_FRAME_WIDTH, 1920);
    cap_.set(CAP_PROP_FRAME_HEIGHT, 1080);
    cap_.set(CAP_PROP_FPS, 30);
    return true;
}

bool BoardDetectorWorker::stepOnce(bool& whiteMoved, QString& boardText) {
    Mat frame;
    cap_ >> frame;
    if (frame.empty()) return false;

    auto now = steady_clock::now();
    double dt = duration_cast<duration<double>>(now - lastDetect_).count();
    if (dt < DETECT_INTERVAL_) return false;
    lastDetect_ = now;

    auto result = system_->tryConfirmState(frame);
    bool confirmed = result.first;
    const BoardState& newState = result.second;
    if (!confirmed) return false;

    BoardState before = system_->getLastConfirmedState();
    whiteMoved = isWhiteMove(before, newState);

    system_->processConfirmedState(newState);

    if (whiteMoved) {
        boardText = boardToText(newState);
        return true;
    }
    return false;
}

// "rnbqkbnr/pppppppp/*******/.../PPPPPPPP/RNBQKBNR"（空き='*', rank8→rank1）
QString BoardDetectorWorker::boardToText(const BoardState& st) const {
    auto pieceChar = [](PieceType p)->char {
        switch (p) {
            case WHITE_PAWN: return 'P';
            case WHITE_KNIGHT: return 'N';
            case WHITE_BISHOP: return 'B';
            case WHITE_ROOK: return 'R';
            case WHITE_QUEEN: return 'Q';
            case WHITE_KING: return 'K';
            case BLACK_PAWN: return 'p';
            case BLACK_KNIGHT: return 'n';
            case BLACK_BISHOP: return 'b';
            case BLACK_ROOK: return 'r';
            case BLACK_QUEEN: return 'q';
            case BLACK_KING: return 'k';
            default: return '*';
        }
    };

    QString out;
    for (int y = 7; y >= 0; --y) {
        for (int x = 0; x < 8; ++x) {
            int idx = y * 8 + x;
            // BoardState の 0 が rank1(上)〜というあなたの定義に合わせる
            out.append(QChar(pieceChar(st.pieces[idx])));
        }
        if (y != 0) out.append('/');
    }
    return out;
}

bool BoardDetectorWorker::isWhiteMove(const BoardState& before, const BoardState& after) const {
    int disappeared = -1;
    for (int i = 0; i < 64; ++i) {
        if (before.pieces[i] != EMPTY && after.pieces[i] == EMPTY) {
            disappeared = i; break;
        }
    }
    if (disappeared < 0) return false;
    auto moved = before.pieces[disappeared];
    return (moved >= WHITE_PAWN && moved <= WHITE_KING);
}
