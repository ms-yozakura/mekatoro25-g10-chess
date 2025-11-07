// main.cpp (最終版)

#include <QApplication>
#include <QDebug>
#include <QMessageBox>
#include <QThread>

// 依存するヘッダ
#include "chess/chess_game.hpp"
#include "main_window.hpp"
#include "serial_manager.hpp" // SerialManager の定義
#include "serial.hpp"

// #ifndef DEV_NAME
// #define DEV_NAME "/dev/cu.usbserial-10"
// #endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 1. ChessGame クラスのインスタンスを作成 (ロジック)
    ChessGame game;

    // // 2. SerialManager のインスタンスを作成 (通信・デバイス)
    // SerialManager serialManager(DEV_NAME);

    // 1. SerialManager オブジェクトを作成
    SerialManager *serialManager = new SerialManager(DEV_NAME);// デバイス名は適切に修正

    // 2. QThread オブジェクトを作成
    QThread *workerThread = new QThread;

    // SerialManager オブジェクトを新しいスレッドに移動！
    serialManager->moveToThread(workerThread);


    // 3. シリアルポートのオープンを試みる (メインスレッドでブロッキング)
    if (!serialManager->openPort())
    {
        // openPort()が失敗した場合、ユーザーに継続するか尋ねる (元のコードのロジックを再現)
        QMessageBox::StandardButton reply = QMessageBox::question(
            nullptr,
            "Serial Connection Failed",
            QString("Could not open serial device: %1. Motor control will be disabled.\n\nDo you want to continue without motor control?").arg(DEV_NAME),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No)
        {
            QMessageBox::information(nullptr, "Exiting", "Application closed by user.");
            return 0;
        }
    }

    // 4. MainWindow のインスタンス化 (GUI)
    // game と serialManager のポインタを渡す
    MainWindow window(&game, serialManager);

    //シリアル通信のスレッド開始
    workerThread->start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, workerThread, &QThread::quit);


    window.show();
    

    // 5. Qtイベントループを開始
    int result = app.exec();

    // 6. アプリケーション終了時に SerialManager のデストラクタが自動的に closePort() を呼び出す

    return result;
}