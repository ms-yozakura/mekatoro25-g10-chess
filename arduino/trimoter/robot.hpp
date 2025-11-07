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

private:
    // モーターピン定義 (元のスケッチから移動)
    // Z軸 (Uni)
    static const int UNI_PIN1 = 4;
    static const int UNI_PIN2 = 5;
    static const int UNI_PIN3 = 6;
    static const int UNI_PIN4 = 7;

    // X軸 (Bip1)
    static const int BIP1_PIN1 = 8;
    static const int BIP1_PIN2 = 9;

    // Y軸 (Bip2)
    static const int BIP2_PIN1 = 10;
    static const int BIP2_PIN2 = 11;

    // モーターインスタンス
    AccelStepper motorUni;
    AccelStepper motorBip1;
    AccelStepper motorBip2;

    // モーター管理配列
    AccelStepper *motors[3];
    const char *axisNames[3];
    static const int NUM_MOTORS = 3;

    // 座標変換定数 (仮設定: 実際の機械定数に合わせる必要があります)
    static constexpr float STEPS_PER_MM_Z = 1000.0;
    static constexpr float STEPS_PER_MM_XY = 500.0;

    // ユーティリティ関数
    int getMotorIndex(char axis);
    void printCoordinates();
};

#endif