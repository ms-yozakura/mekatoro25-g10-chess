#include <AccelStepper.h>

// --- (1) モーターピン定義 ---
// 既存のユニポーラモーター (D4, D5, D6, D7を使用)
#define UNI_PIN1 4
#define UNI_PIN2 5
#define UNI_PIN3 6
#define UNI_PIN4 7

// バイポーラモーター1 (DRV8835 - 4線制御)
// DRV8835のIN1/IN2/IN3/IN4ピンに対応
#define BIP1_PIN1 8  // IN1 (Motor A)
#define BIP1_PIN2 9  // IN2 (Motor A)
#define BIP1_PIN3 A0 // IN3 (Motor B) *未使用の場合は0でも可
#define BIP1_PIN4 A1 // IN4 (Motor B) *未使用の場合は0でも可

// バイポーラモーター2 (DRV8835 - 4線制御)
// 別のDRV8835チップを使用、または同じDRV8835チップの残り2ピンを使用する場合は調整
#define BIP2_PIN1 10 // IN1 (Motor A)
#define BIP2_PIN2 11 // IN2 (Motor A)
#define BIP2_PIN3 A2 // IN3 (Motor B) *未使用の場合は0でも可
#define BIP2_PIN4 A3 // IN4 (Motor B) *未使用の場合は0でも可

// --- (2) AccelStepper インスタンス作成 ---
// 既存のユニポーラモーター (4線ハーフステップ推奨)
// (ピン順序は既存コードに合わせていますが、実機に合わせて調整が必要です)
AccelStepper motorUni(AccelStepper::FULL4WIRE, UNI_PIN1, UNI_PIN3, UNI_PIN4, UNI_PIN2);

// バイポーラモーター1 (DRV8835 - 4線フルステップ)
// 4線モードとして、2つの出力ピン(IN1, IN2)でコイルを制御
// **注意**: DRV8835では、ステップ信号を適切に生成するためにこの定義で動作するかテストが必要です。
// 動作しない場合は、ステップ/ディレクションモード(AccelStepper::DRIVER)用にIN1/IN2を使い、
// パルスと方向の制御をDRV8835の動作モードに合わせて別途実装する必要があります。
AccelStepper motorBip1(AccelStepper::FULL4WIRE, BIP1_PIN1, 0, BIP1_PIN2, 0);

// バイポーラモーター2
AccelStepper motorBip2(AccelStepper::FULL4WIRE, BIP2_PIN1, 0, BIP2_PIN2, 0);

// 全モーターを配列に格納してループ処理を可能にする
AccelStepper *motors[] = {&motorUni, &motorBip1, &motorBip2};
const char *axisNames[] = {"z", "x", "y"}; // コマンドで使う軸名
const int NUM_MOTORS = 3;

// --- (3) シリアル通信・コマンド処理変数 ---
const int maxCommandLength = 32;
char serialBuffer[maxCommandLength];
int bufferIndex = 0;

void setup()
{
    Serial.begin(9600);
    Serial.println("Ready to receive commands (ON/OFF/MOVE(x,y)/HOME)");

    // モーターの初期設定
    for (int i = 0; i < NUM_MOTORS; i++)
    {               // Uモーターのみ速度を制限
        if (i == 0) // uni
        {
            motors[i]->setMaxSpeed(500.0);
            motors[i]->setAcceleration(10000.0);
        }
        else // bi
        {
            // B1, B2 (バイポーラモーター) の速度は高いままにする
            motors[i]->setMaxSpeed(4000.0);
            motors[i]->setAcceleration(2000.0);
        }
    }
}

void loop()
{
    // --- (A) モーター駆動処理 ---
    // AccelStepperはこれをループで呼び出すだけで非ブロッキングに動作
    for (int i = 0; i < NUM_MOTORS; i++)
    {
        motors[i]->run();
    }

    // --- (B) シリアル受信処理 ---
    while (Serial.available())
    {
        char incomingChar = Serial.read();
        if (incomingChar == '\n' || incomingChar == '\r')
        {
            serialBuffer[bufferIndex] = '\0';
            processCommand(serialBuffer);
            bufferIndex = 0;
            memset(serialBuffer, 0, maxCommandLength);
        }
        else if (bufferIndex < maxCommandLength - 1)
        {
            serialBuffer[bufferIndex++] = incomingChar;
        }
    }
}

// --- (C) コマンド処理関数の大幅な変更 ---
void processCommand(const char *command)
{
    char axis = ' ';
    int targetPos = 0;

    // 1. ON+ (正転開始) / ON- (逆転開始) コマンド
    // フォーマット例: ON+(U)
    if (strncmp(command, "ON+", 3) == 0 || strncmp(command, "ON-", 3) == 0)
    {
        if (sscanf(command + 4, "%c)", &axis) == 1)
        {
            int motorIndex = -1;
            for (int i = 0; i < NUM_MOTORS; i++)
            {
                if (axis == axisNames[i][0])
                {
                    motorIndex = i;
                    break;
                }
            }

            if (motorIndex != -1)
            {
                Serial.print(axis);
                Serial.print("-Motor ON");
                // 無限回転のための目標設定 (非常に大きな数値)
                long current = motors[motorIndex]->currentPosition();
                long target = (command[2] == '+') ? current + 1000000000L : current - 1000000000L;
                motors[motorIndex]->moveTo(target);
                motors[motorIndex]->setSpeed(motors[motorIndex]->maxSpeed());
                Serial.println((command[2] == '+') ? "+" : "-");
                return;
            }
        }
    }

    // 2. OFF (停止) コマンド
    // フォーマット例: OFF(B1)
    if (strncmp(command, "OFF(", 4) == 0)
    {
        if (sscanf(command + 4, "%c)", &axis) == 1)
        {
            int motorIndex = -1;
            if (axis == "a") // all
            {
                Serial.println("All Motor OFF (Decelerating)");
                for (const auto &motor : motors)
                {
                    motor->stop();
                    return;
                }
            }
            for (int i = 0; i < NUM_MOTORS; i++)
            {
                if (axis == axisNames[i][0])
                {
                    motorIndex = i;
                    break;
                }
            }

            if (motorIndex != -1)
            {
                Serial.print(axis);
                Serial.println("-Motor OFF (Decelerating)");
                motors[motorIndex]->stop(); // 加速/減速制御で停止
                return;
            }
        }
    }

    // 3. MOVE (絶対位置移動) コマンド
    // フォーマット例: MOVE(B1,1000) -> B1モーターをステップ1000の位置へ移動
    if (strncmp(command, "MOVE(", 5) == 0)
    {
        // MOVE(軸名,ステップ数) のフォーマットで解析
        if (sscanf(command + 5, "%c,%d)", &axis, &targetPos) == 2)
        {
            int motorIndex = -1;
            for (int i = 0; i < NUM_MOTORS; i++)
            {
                if (axis == axisNames[i][0])
                {
                    motorIndex = i;
                    break;
                }
            }

            if (motorIndex != -1)
            {
                Serial.print("MOVE Motor ");
                Serial.print(axis);
                Serial.print(" to position: ");
                Serial.println(targetPos);
                motors[motorIndex]->moveTo(targetPos);
                return;
            }
        }
    }

    // 4. HOME (原点復帰/位置リセット) コマンド
    if (strcmp(command, "HOME") == 0)
    {
        Serial.println("0,0");
        // 全モーターの位置をリセット
        for (int i = 0; i < NUM_MOTORS; i++)
        {
            motors[i]->setCurrentPosition(0);
        }
        return;
    }

    // 5. UP/DOWN/WARP などの既存のダミーコマンドは、必要に応じて上記のロジックに合わせて実装してください。
    // ここでは既存のSerial.print()の動作を維持します。

    if (strcmp(command, "UP") == 0)
    {
        Serial.println("Z-Magnet UP: ON+z equivalent");
        return;
    }

    if (strcmp(command, "DOWN") == 0)
    {
        Serial.println("Z-Magnet DOWN: OFFz equivalent");
        return;
    }

    if (strncmp(command, "WARP(", 5) == 0 && command[strlen(command) - 1] == ')')
    {
        int x, y;
        if (sscanf(command + 5, "%f,%f)", &x, &y) == 2)
        {
            Serial.print("WARP to ");
            Serial.print(x);
            Serial.print(",");
            Serial.println(y);
            return;
        }
    }

    Serial.print("Error: Unknown command or invalid format: ");
    Serial.println(command);
}