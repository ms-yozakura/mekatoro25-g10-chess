#include "robot.hpp"  // 新しいヘッダーファイルをインクルード
#include <string.h> // memsetのためにインクルード

// --- (1) Robot インスタンス作成 ---
Robot robot;

// --- (2) シリアル通信・コマンド処理変数 ---
const int maxCommandLength = 32;
char serialBuffer[maxCommandLength];
int bufferIndex = 0;

void setup()
{
    robot.begin(); // Robotクラスの初期化関数を呼び出す
}

void loop()
{
    // モーター駆動処理
    robot.runMotors();

    // シリアル受信処理
    while (Serial.available())
    {
        char incomingChar = Serial.read();
        if (incomingChar == '\n' || incomingChar == '\r')
        {
            serialBuffer[bufferIndex] = '\0';
            robot.processCommand(serialBuffer); // Robotクラスのコマンド処理を呼び出す
            bufferIndex = 0;
            memset(serialBuffer, 0, maxCommandLength);
        }
        else if (bufferIndex < maxCommandLength - 1)
        {
            serialBuffer[bufferIndex++] = incomingChar;
        }
    }
}