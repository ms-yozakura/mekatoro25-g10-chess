// serial_manager.cpp

#include "serial_manager.hpp"
#include <QDebug>
#include <unistd.h>  // open, close
#include <fcntl.h>   // open のフラグ
#include <termios.h> // シリアルポート設定
#include <errno.h>   // エラー処理
#include <cstdio>    // perror
#include <sstream>
#include "serial.hpp" // 既存の serial_init と send_and_receive の関数宣言
#include <QThread>

// コンストラクタ
SerialManager::SerialManager(const std::string &dev_name, QObject *parent)
    : QObject(parent), m_devName(dev_name)
{
    // 初期化は openPort() で行う
}

// デストラクタ: ポートを確実にクローズ
SerialManager::~SerialManager()
{
    closePort();
}

// ポートを開く処理 (元の main.cpp のロジック)
bool SerialManager::openPort()
{
    if (m_fd >= 0)
    {
        qDebug() << "Port already open (fd:" << m_fd << ")";
        return true;
    }

    qDebug() << "Attempting to open serial device:" << m_devName.c_str();

    // デバイスファイル（シリアルポート）オープン
    m_fd = open(m_devName.c_str(), O_RDWR | O_NOCTTY);

    if (m_fd < 0)
    {
        QString errorMsg = QString("ERROR: Could not open device %1. %2").arg(m_devName.c_str()).arg(strerror(errno));
        qDebug() << errorMsg;

        emit serialError(errorMsg); // GUIにエラーを通知
        emit portStatusChanged(false);
        return false;
    }
    else
    {
        qDebug() << "Serial port opened successfully (fd:" << m_fd << ").";

        // 初期設定
        serial_init(m_fd);
        // sleep(2); // GUIスレッドをブロックしないよう、ここでは省略または非同期処理を検討すべきだが、元のコードに合わせて一旦残さない
        tcflush(m_fd, TCIOFLUSH);

        emit portStatusChanged(true); // GUIに成功を通知
        return true;
    }
}

// ポートを閉じる処理
void SerialManager::closePort()
{
    if (m_fd >= 0)
    {
        ::close(m_fd); // ::close はシステムの close を明示的に呼ぶ
        m_fd = -1;
        m_moter_isON[0] = false;
        m_moter_isON[1] = false;
        m_moter_isON[2] = false;
        qDebug() << "Serial port closed.";
        emit portStatusChanged(false);
        emit motorStatusChanged('x', false);
        emit motorStatusChanged('y', false);
        emit motorStatusChanged('z', false);
    }
}

// --- コマンド送信 ---
void SerialManager::sendCommand(const std::string &command)
{
    if (m_fd < 0)
    {
        emit serialError("Serial port is not open. Cannot send command: " + QString::fromStdString(command));
        return;
    }

    // 既存の関数 send_and_receive を利用
    send_and_receive(m_fd, command.c_str());
}

// --- 公開された操作メソッド ---

void SerialManager::sendOnPlus(char axis)
{
    sendCommand(std::string("ON+(") + axis+')');

    switch (axis)
    {
    case 'x':
        m_moter_isON[0] = true;
        break;
    case 'y':
        m_moter_isON[1] = true;
        break;
    case 'z':
        m_moter_isON[2] = true;
        break;
    }
    emit motorStatusChanged(axis, true);
}

void SerialManager::sendOnMinus(char axis)
{
    sendCommand(std::string("ON-(") + axis+')');

    switch (axis)
    {
    case 'x':
        m_moter_isON[0] = true;
        break;
    case 'y':
        m_moter_isON[1] = true;
        break;
    case 'z':
        m_moter_isON[2] = true;
        break;
    }
    emit motorStatusChanged(axis, true);
}

void SerialManager::sendOff(char axis)
{
    sendCommand(std::string("OFF(") + axis+')');

    switch (axis)
    {
    case 'x':
        m_moter_isON[0] = false;
        break;
    case 'y':
        m_moter_isON[1] = false;
        break;
    case 'z':
        m_moter_isON[2] = false;
        break;
    case 'a':
        m_moter_isON[0] = false;
        m_moter_isON[1] = false;
        m_moter_isON[2] = false;
        break;
    }
    emit motorStatusChanged(axis, false);
}

void SerialManager::sendCalib()
{
    sendCommand("CALIB");
    // キャリブレーションがモーターON/OFFの状態を変えるなら、ここで m_moter_isON を更新
    // 今回はコマンドを送るだけとする
}

// void SerialManager::sendLongCommand(const std::string &command)
// {
//     std::stringstream ss(command); // ストリームに変換
//     std::string segment;

//     // 2. std::getline を使って、改行('\n')ごとに文字列を抽出
//     while (std::getline(ss, segment, '\n'))
//     {
//         // segmentが空でなく、末尾の文字が '\r' なら削除する
//         if (!segment.empty() && segment.back() == '\r')
//         {
//             segment.pop_back();
//         }
//         // コメント行（"//"で始まる行）と空のセグメントを除外
//         if (!segment.empty() && segment.find("//") != 0)
//         {
//             // コマンドを SerialManager に送信
//             // std::string をそのまま sendCommand に渡す
//             sendCommand(segment);
//         }
//     }
// }


void SerialManager::processLongCommandSlot(const QString &command)
{
    if (m_fd < 0)
    {
        emit serialError("Serial port is not open. Cannot send command: " + command);
        return;
    }
    
    // QStringをstd::stringに変換
    std::string commandStr = command.toStdString();

    std::stringstream ss(commandStr); // ストリームに変換
    std::string segment;

    // 2. std::getline を使って、改行('\n')ごとに文字列を抽出
    while (std::getline(ss, segment, '\n'))
    {
        // segmentが空でなく、末尾の文字が '\r' なら削除する
        if (!segment.empty() && segment.back() == '\r')
        {
            segment.pop_back();
        }
        // コメント行（"//"で始まる行）と空のセグメントを除外
        if (!segment.empty() && segment.find("//") != 0)
        {
            // sendCommandが send_and_receive (usleepを含む) を呼び出す
            sendCommand(segment); 
        }
    }
}