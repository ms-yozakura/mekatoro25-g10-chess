

// serial.hpp

#pragma once

// C言語標準の入出力と型定義のみを残す
#include <termios.h> // BAUD_RATE, B9600 などのために必要

// ★★★ 環境に合わせてここを修正してください ★★★
// Mac/Linuxの場合: Arduinoのポート名を設定
#define DEV_NAME "/dev/cu.usbserial-10"
#define BAUD_RATE B9600
#define BUFF_SIZE 256 // バッファサイズ

// シリアルポートの初期化関数
// (定義は serial.cpp に移動済みと仮定)
int serial_init(int fd); // int を返すように変更するとエラー処理がしやすくなります

// コマンド送信＆応答受信関数
// (定義は serial.cpp に移動済みと仮定)
void send_and_receive(int fd, const char *command);