#ifndef ROBOT_H
#define ROBOT_H

#include <AccelStepper.h>
#include <Arduino.h> // Arduino固有の型（byteなど）や定数（HIGH/LOW）のために念のためインクルード

class Robot
{
public:
    // コンストラクタ
    Robot();

    // 初期化関数 (setup() で呼び出す)
    void begin();

    // ループ関数 (loop() で呼び出す)
    void runMotors();

    // コマンド処理関数 (Serial受信後に呼び出す)
    void processCommand(const char *command);

    void getCoordinates(float *coord_out);

    bool isCommandQueueEmpty() const; // キューが空かチェック
private:
    // モーターピン定義 (元のスケッチから移動)
    // Z軸 (Uni)
    static const int UNI_PIN1 = 4;
    static const int UNI_PIN2 = 5;
    static const int UNI_PIN3 = 6;
    static const int UNI_PIN4 = 7;

    // X軸 (Bip1)
    static const int BIP1_PIN1 = 2;
    static const int BIP1_PIN2 = 3;

    // Y軸 (Bip2)
    static const int BIP2_PIN1 = 8;
    static const int BIP2_PIN2 = 9;

    // モーターインスタンス
    AccelStepper motorUni;
    AccelStepper motorBip1;
    AccelStepper motorBip2;

    // モーター管理配列
    AccelStepper *motors[3];
    const char *axisNames[3];
    static const int NUM_MOTORS = 3;

    // 座標変換定数 (仮設定: 実際の機械定数に合わせる必要があります)
    static constexpr float STEPS_PER_MM_Z = 60.0;
    static constexpr float STEPS_PER_MM_XY = 2.0;

    // --- ★ キュー関連の追加 ---
    static const int MAX_COMMANDS = 10; // キューの最大サイズ
    struct QueuedCommand
    {
        char commandString[32];   // コマンド文字列を格納
        bool isExecuting = false; // 現在実行中か
    };
    QueuedCommand commandQueue[MAX_COMMANDS];
    int queueHead = 0; // 次に追加する位置 (キューの末尾)
    int queueTail = 0; // 次に実行するコマンドの位置 (キューの先頭)

    // 現在実行中のコマンドの状態
    bool isCommandExecuting = false; // モーター駆動を伴うコマンドが実行中か

    // ユーティリティ関数
    int getMotorIndex(char axis);
    void printCoordinates();

    // --- ★ 新しいプライベートメソッド ---
    bool enqueueCommand(const char *command); // コマンドをキューに追加
    void executeNextCommand();                // キューの次のコマンドを実行開始
};

#endif