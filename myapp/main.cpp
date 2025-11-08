// main.cpp (修正版)

#include <QApplication>
#include <QMessageBox>
#include <QThread>
#include <QDebug>

// 依存ヘッダ
#include "chess/chess_game.hpp"
#include "main_window.hpp"
#include "serial_manager.hpp"
#include "serial.hpp"
#include "board_detector_worker.hpp"   // ★ 追加：ビジョンワーカー

// デバイス名は環境に合わせて
#ifndef DEV_NAME
#define DEV_NAME "/dev/cu.usbserial-10"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 1) ロジック（チェスエンジン）
    ChessGame game;

    // 2) シリアル（まずはメインスレッドで生成＆open）
    SerialManager *serialManager = new SerialManager(DEV_NAME);
    if (!serialManager->openPort()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "Serial Connection Failed",
            QString("Could not open serial device: %1.\n\n"
                    "Continue without motor control?").arg(DEV_NAME),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::No) {
            QMessageBox::information(nullptr, "Exiting", "Application closed by user.");
            return 0;
        }
        // 以降、ポート未接続でもGUIは動く想定
    }

    // 3) GUI
    MainWindow window(&game, serialManager);
    window.show();

    // 4) SerialManager を専用スレッドへ移動（openPort 済みなのでOK）
    QThread *serialThread = new QThread;
    serialManager->moveToThread(serialThread);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, serialThread, &QThread::quit);
    QObject::connect(serialThread, &QThread::finished, serialManager, &QObject::deleteLater);
    QObject::connect(serialThread, &QThread::finished, serialThread, &QObject::deleteLater);
    serialThread->start();

    // 5) Vision ワーカー（カメラ認識）を別スレッドで起動
    auto *visionWorker = new BoardDetectorWorker(
        /*calibPath=*/QStringLiteral("calib.yaml"),
        /*device=*/QString(),   // 文字列パスを使う場合はここに指定
        /*camIndex=*/0          // 数値インデックスを使うならこれ
    );
    QThread *visionThread = new QThread;
    visionWorker->moveToThread(visionThread);

    // スレッド開始で run()
    QObject::connect(visionThread, &QThread::started, visionWorker, &BoardDetectorWorker::run);
    // 終了シーケンス
    QObject::connect(&app, &QCoreApplication::aboutToQuit, visionWorker, &BoardDetectorWorker::stop);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, visionThread, &QThread::quit);
    QObject::connect(visionThread, &QThread::finished, visionWorker, &QObject::deleteLater);
    QObject::connect(visionThread, &QThread::finished, visionThread, &QObject::deleteLater);

    // 6) Vision → MainWindow の接続（白が動いたら盤面文字列を受け取る）
    QObject::connect(
        visionWorker, &BoardDetectorWorker::boardForWhite,
        &window,      &MainWindow::onVisionBoard,
        Qt::QueuedConnection
    );
    // 任意：ログ
    QObject::connect(visionWorker, &BoardDetectorWorker::status,
                     &window,      &MainWindow::onStatusLog, Qt::QueuedConnection);
    QObject::connect(visionWorker, &BoardDetectorWorker::error,
                     &window,      &MainWindow::onErrorLog, Qt::QueuedConnection);

    // Vision スレッド開始
    visionThread->start();

    // 7) イベントループ
    const int result = app.exec();
    return result;
}
