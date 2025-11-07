#include "robot.hpp"

// Robotクラスのインスタンスを作成
Robot robot;

void setup()
{
    // Robotクラスの初期化関数を呼び出す
    robot.begin();
}

void loop()
{
    // --- (A) モーター駆動処理 ---
    robot.runMotors();

    // --- (B) シリアル受信処理 ---
    // trimoter.ino の元のロジックを流用し、
    // 受信したコマンドを robot.processCommand() に渡す
    static const int maxCommandLength = 32;
    static char serialBuffer[maxCommandLength];
    static int bufferIndex = 0;

    while (Serial.available())
    {
        char incomingChar = Serial.read();

        if (incomingChar == '\n' || incomingChar == '\r')
        {
            if (bufferIndex > 0)
            {
                serialBuffer[bufferIndex] = '\0';
                robot.processCommand(serialBuffer); // ★ Robotインスタンスのメソッドを呼び出し
                bufferIndex = 0;
                memset(serialBuffer, 0, maxCommandLength);
            }
        }
        else if (bufferIndex < maxCommandLength - 1)
        {
            serialBuffer[bufferIndex++] = incomingChar;
        }
    }
}