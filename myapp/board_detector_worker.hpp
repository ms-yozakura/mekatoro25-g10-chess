#pragma once
#include <QObject>
#include <QAtomicInt>
#include <QString>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <memory>

class ChessDetectionSystem;

// BoardDetectorWorker は別スレッドで動作し、白手が確定したタイミングで
// rnbqkbnr/... 形式の QString を signal で通知します。
class BoardDetectorWorker : public QObject {
    Q_OBJECT
public:
    explicit BoardDetectorWorker(QString calibPath,
                                 QString device = {},
                                 int camIndex = 0,
                                 QObject* parent = nullptr);
    ~BoardDetectorWorker();

public slots:
    void run();   // QThread::started に接続
    void stop();  // アプリ終了時などに停止

signals:
    void boardForWhite(const QString& boardText); // ★ 白が動いた直後の盤面文字列を通知
    void status(const QString& msg);              // ログ用（任意）
    void error(const QString& msg);               // エラー用（任意）

private:
    // 初期化と 1 ステップ実行
    bool initSystem();
    bool openCamera();
    bool stepOnce(bool& whiteMoved, QString& boardText);

    // 盤面出力と「誰が動いたか」判定
    QString boardToText(const struct BoardState& st) const;
    bool isWhiteMove(const struct BoardState& before, const struct BoardState& after) const;

    // 設定
    QString calibPath_, device_;
    int camIndex_;
    const double DETECT_INTERVAL_ = 0.5; // 秒

    // 状態
    QAtomicInt running_{1};
    cv::VideoCapture cap_;
    std::unique_ptr<ChessDetectionSystem> system_;
    std::chrono::steady_clock::time_point lastDetect_{};
};
