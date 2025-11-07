// serial.cpp

// C言語/OSレベルのシリアル通信に必要なヘッダー
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <cstdio> // snprintf, perror のため

// Qtのデバッグ出力のため（リファクタリング構造に合わせる）
#include <QDebug>

// 関数宣言とマクロ定義を含むヘッダー
// DEV_NAME, BAUD_RATE, BUFF_SIZE がここから提供される
#include "serial.hpp"

// シリアルポートの初期化関数
int serial_init(int fd)
{
    qDebug() << "Initializing serial port (fd:" << fd << ") with Baud Rate" << BAUD_RATE;

    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    // CREAD: 受信有効, CLOCAL: モデム制御を無視, CS8: 8ビットデータ
    tio.c_cflag = CS8 | CLOCAL | CREAD;

    // 入出力ボーレート設定
    cfsetispeed(&tio, BAUD_RATE);
    cfsetospeed(&tio, BAUD_RATE);

    // TTY制御文字の設定
    // VMIN: 読み込みたい文字の最小数 (0), VTIME: 読み込みのタイムアウト時間 (0.1秒)
    tio.c_cc[VMIN] = 0;
    // VTIMEを10（1秒）に設定。データがなくても最大1秒待機する。
    tio.c_cc[VTIME] = 10;

    // ローカルフラグ: Rawモードに近い設定
    // ICANON, ECHO, ECHOE, ISIGを無効にし、文字をそのまま処理する
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // デバイスに設定を行う (TCSANOW: 変更を即座に適用)
    if (tcsetattr(fd, TCSANOW, &tio) < 0)
    {
        qDebug() << "ERROR setting serial attributes:" << strerror(errno);
    }
    return 0;
}

// コマンド送信＆応答受信関数
void send_and_receive(int fd, const char *command)
{
    // fdの有効性チェックは SerialManager で行われるため、簡易化
    if (fd < 0)
        return;

    // BUFF_SIZE は serial.hpp から提供される
    char tx_buffer[BUFF_SIZE];
    char rx_buffer[BUFF_SIZE];
    ssize_t len;

    // 1. コマンドに改行文字('\n')を付けて送信バッファを準備
    // BUFF_SIZE を使ってバッファオーバーフローを防ぐ
    snprintf(tx_buffer, BUFF_SIZE, "%s\n", command);

    qDebug() << "Sending:" << tx_buffer;

    // 2. データの送信
    len = write(fd, tx_buffer, strlen(tx_buffer));
    if (len < 0)
    {
        qDebug() << "ERROR writing to serial port:" << strerror(errno);
        return;
    }

    // 3. 送信後、Arduinoが処理するのを待つ (500ms待機)
    usleep(500000);

    // 4. 応答の受信
    memset(rx_buffer, 0, BUFF_SIZE); // 受信バッファをクリア

    // VTIME (1.0秒) に基づき読み取りを試みる
    len = read(fd, rx_buffer, BUFF_SIZE - 1);

    if (len > 0)
    {
        rx_buffer[len] = '\0'; // ヌル終端
        // 受信データから改行を削除して表示（任意）
        size_t newline_pos = strcspn(rx_buffer, "\r\n");
        rx_buffer[newline_pos] = '\0';

        qDebug() << "Received:" << rx_buffer;
    }
    else if (len == 0)
    {
        qDebug() << "Received: (No response in time)";
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        qDebug() << "ERROR reading from serial port:" << strerror(errno);
    }
    qDebug() << "--------------------------------";
}
