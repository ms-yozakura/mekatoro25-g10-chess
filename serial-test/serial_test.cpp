#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <errno.h> // エラー処理のために追加

// ★★★ 環境に合わせてここを修正してください ★★★
// Mac/Linuxの場合: Arduinoのポート名を設定
// /dev/cu.usbserial-XXX は安定していますが、/dev/tty.usbserial-XXX も試せます。
#define DEV_NAME "/dev/cu.usbserial-10" 
#define BAUD_RATE B9600
#define BUFF_SIZE 256 // バッファサイズ

// シリアルポートの初期化関数
void serial_init(int fd) {
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
    tio.c_cc[VTIME] = 10; // 10 * 0.1秒 = 1秒 (非ブロックモードではないので、最長1秒待機)

    // ローカルフラグ: Rawモードに近い設定
    // ICANON, ECHO, ISIGを無効にし、文字をそのまま処理する
    tio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); 
    
    // デバイスに設定を行う (TCSANOW: 変更を即座に適用)
    tcsetattr(fd, TCSANOW, &tio);
}

// コマンド送信＆応答受信関数
void send_and_receive(int fd, const char* command) {
    char tx_buffer[BUFF_SIZE];
    char rx_buffer[BUFF_SIZE];
    int len;
    
    // 1. コマンドに改行文字('\n')を付けて送信バッファを準備
    snprintf(tx_buffer, BUFF_SIZE, "%s\n", command);
    
    printf("Sending: %s", tx_buffer);

    // 2. データの送信
    len = write(fd, tx_buffer, strlen(tx_buffer));
    if (len < 0) {
        perror("ERROR writing to serial port");
        return;
    }
    
    // 3. 送信後、Arduinoが処理するのを待つ (ディレイを延長)
    // 応答の安定性を向上させるため、ディレイを500msに延長しました。
    usleep(500000); // 500ms待機 
    
    // 4. 【削除】コマンドごとの受信バッファクリアは削除しました。
    
    // 5. 応答の受信
    memset(rx_buffer, 0, BUFF_SIZE); // 受信バッファをクリア
    
    // readはノンブロッキングではないため、データが来るかVTIMEで設定した時間まで待つ
    // usleepで既に時間が経過しているので、バッファにデータがあれば読み取ります。
    len = read(fd, rx_buffer, BUFF_SIZE - 1); 
    
    if (len > 0) {
        rx_buffer[len] = '\0'; // ヌル終端
        printf("Received: %s", rx_buffer); // 受信データを表示
    } else if (len == 0) {
        printf("Received: (No response in time)\n");
    } else if (errno != EAGAIN) {
        // EAGAIN (EWOULDBLOCK) 以外は真のI/Oエラー
        perror("ERROR reading from serial port"); 
    }
    printf("--------------------------------\n");
}


int main() {
    int fd;
    
    printf("Start serial communication example.\n");

    // デバイスファイル（シリアルポート）オープン
    // O_RDWR: 読み書き可能, O_NOCTTY: ターミナルとして扱わない
    fd = open(DEV_NAME, O_RDWR | O_NOCTTY ); 
    
    if(fd < 0) { 
        printf("ERROR: Could not open device %s\n", DEV_NAME);
        perror("open"); // open関数のエラー詳細を出力
        exit(1);
    }
    
    printf("Serial port opened successfully.\n");
    
    // シリアルポートの初期化
    serial_init(fd);

    // Arduinoがリセットから起動するのを待つためのディレイ (2秒で初期メッセージを送り終えるのを待つ)
    sleep(2); 
    
    // ★★★ 起動後の残っているデータ（Arduinoの初期メッセージなど）をすべて破棄 ★★★
    tcflush(fd, TCIOFLUSH); 

    // ★★★ コマンドの送信と応答の受信 ★★★
    
    // 1. MOVEコマンドの送信
    send_and_receive(fd, "MOVE(100,200)");
    
    // 2. HOMEコマンドの送信
    send_and_receive(fd, "HOME");

    // 3. 無効なコマンドの送信
    send_and_receive(fd, "TEST");


    send_and_receive(fd, "OFF");

    // デバイスファイルをクローズ
    close(fd); 
    printf("Serial port closed. End of program.\n");

    return 0;
}
