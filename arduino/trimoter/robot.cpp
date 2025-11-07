#include "robot.hpp"
#include <string.h> // strncmp, strcmp, sscanfのために必要

// コンストラクタ: モーターインスタンスの初期化と配列への格納
Robot::Robot()
    // AccelStepper インスタンスの初期化
    // Z軸 (Uni): FULL4WIRE (ピン順序は元のコードから維持)
    : motorUni(AccelStepper::FULL4WIRE, UNI_PIN1, UNI_PIN3, UNI_PIN4, UNI_PIN2),
      // X軸 (Bip1): FULL4WIRE (ピン3,4は0)
      motorBip1(AccelStepper::FULL4WIRE, BIP1_PIN1, 0, BIP1_PIN2, 0),
      // Y軸 (Bip2): FULL4WIRE (ピン3,4は0)
      motorBip2(AccelStepper::FULL4WIRE, BIP2_PIN1, 0, BIP2_PIN2, 0)
{
    // モーター配列と軸名の設定
    motors[0] = &motorUni;
    motors[1] = &motorBip1;
    motors[2] = &motorBip2;
    axisNames[0] = "z";
    axisNames[1] = "x";
    axisNames[2] = "y";
}

// 初期設定: Serial開始とMaxSpeed/Acceleration の設定
void Robot::begin()
{
    Serial.begin(9600);
    Serial.println("Robot System Ready (CALIB/HOME/ON+/OFF/MOVE/COORD)");

    for (int i = 0; i < NUM_MOTORS; i++)
    {
        if (i == 0) // Z軸 (motorUni)
        {
            motors[i]->setMaxSpeed(500.0);
            motors[i]->setAcceleration(10000.0);
        }
        else // X, Y軸 (motorBip1, motorBip2)
        {
            motors[i]->setMaxSpeed(4000.0);
            motors[i]->setAcceleration(2000.0);
        }
    }
}

// モーター駆動 (loop()内で常に呼び出す)
void Robot::runMotors()
{
    for (int i = 0; i < NUM_MOTORS; i++)
    {
        motors[i]->run();
    }
}

// 軸名からモーター配列のインデックスを取得
int Robot::getMotorIndex(char axis)
{
    for (int i = 0; i < NUM_MOTORS; i++)
    {
        if (axis == axisNames[i][0])
        {
            return i;
        }
    }
    return -1;
}

void Robot::getCoordinates(float *coord_out)
{
    long z_steps = motors[0]->currentPosition();
    long x_steps = motors[1]->currentPosition();
    long y_steps = motors[2]->currentPosition();

    // ステップ数を座標(mm)に変換
    float z_coord = (float)z_steps / STEPS_PER_MM_Z;
    float x_coord = (float)x_steps / STEPS_PER_MM_XY;
    float y_coord = (float)y_steps / STEPS_PER_MM_XY;

    coord_out[0] = x_coord;
    coord_out[1] = y_coord;
    coord_out[2] = z_coord;
}

// 現在座標のシリアル出力 (mm単位)
void Robot::printCoordinates()
{
    long z_steps = motors[0]->currentPosition();
    long x_steps = motors[1]->currentPosition();
    long y_steps = motors[2]->currentPosition();

    // ステップ数を座標(mm)に変換
    float z_coord = (float)z_steps / STEPS_PER_MM_Z;
    float x_coord = (float)x_steps / STEPS_PER_MM_XY;
    float y_coord = (float)y_steps / STEPS_PER_MM_XY;

    // シリアルに出力
    Serial.print("COORD Z:");
    Serial.print(z_coord, 2);
    Serial.print(", X:");
    Serial.print(x_coord, 2);
    Serial.print(", Y:");
    Serial.println(y_coord, 2);
}

// コマンド処理
void Robot::processCommand(const char *command)
{
    char axis = ' ';
    int targetPos = 0;
    int motorIndex = -1;

    char tempCommand[32];
    strncpy(tempCommand, command, 31);
    tempCommand[31] = '\0';

    // 1. ON+ (正転開始) / ON- (逆転開始) コマンド
    if (strncmp(command, "ON+", 3) == 0 || strncmp(command, "ON-", 3) == 0)
    {
        if (sscanf(command + 4, "%c)", &axis) == 1)
        {
            motorIndex = getMotorIndex(axis);

            if (motorIndex != -1)
            {
                Serial.print(axis);
                Serial.print("-Motor ON");
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
    if (strncmp(command, "OFF(", 4) == 0)
    {
        float current_coords[3];
        getCoordinates(current_coords);

        if (sscanf(command + 4, "%c)", &axis) == 1)
        {
            if (axis == 'a') // all
            {
                Serial.print(F("All Motor OFF (Decelerating): ("));
                Serial.print(current_coords[0], 2);
                Serial.print(F(","));
                Serial.print(current_coords[1], 2);
                Serial.print(F(","));
                Serial.print(current_coords[2], 2);
                Serial.println(F(")"));
                for (const auto &motor : motors)
                {
                    motor->stop();
                }
                return;
            }

            motorIndex = getMotorIndex(axis);

            if (motorIndex != -1)
            {
                Serial.print(axis);
                Serial.print(F("-Motor OFF (Decelerating): ("));
                Serial.print(current_coords[0], 2);
                Serial.print(F(","));
                Serial.print(current_coords[1], 2);
                Serial.print(F(","));
                Serial.print(current_coords[2], 2);
                Serial.println(F(")"));
                motors[motorIndex]->stop();
                return;
            }
        }
    }

    // 4. CALIB (キャリブレーション/初期化) コマンド: 全モーターの位置を0にリセット
    if (strcmp(command, "CALIB") == 0)
    {
        Serial.println("CALIB: All Motor Positions Reset (0 steps)");
        for (int i = 0; i < NUM_MOTORS; i++)
        {
            motors[i]->setCurrentPosition(0);
        }
        return;
    }

    // 5. HOME (原点復帰/動作) コマンド: X=0, Y=0, Zを停止
    if (strcmp(command, "HOME") == 0)
    {
        Serial.println("HOME: Moving to X=0, Y=0, and Z-Down (OFF(z))");
        motors[1]->moveTo(0); // X軸 (motorBip1) を絶対位置0へ移動
        motors[2]->moveTo(0); // Y軸 (motorBip2) を絶対位置0へ移動
        motors[0]->stop();    // Z軸 (motorUni) を減速停止 (DOWN動作と見なす)
        return;
    }

    if (strcmp(command, "COORD") == 0)
    {
        float current_coords[3];
        getCoordinates(current_coords); // 新しいメソッドで座標を取得

        Serial.print("COORD X:");
        Serial.print(current_coords[0], 2);
        Serial.print(", Y:");
        Serial.print(current_coords[1], 2);
        Serial.print(", Z:");
        Serial.println(current_coords[2], 2);
        return;
    }

    // 7. ダミーコマンドの維持
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

    // 7. WARP (線形補間移動) コマンドの修正
    // フォーマット例: WARP(100.5,50) -> X座標100.5mm、Y座標50mmへ同時到達で移動
    // NOTE: sscanfの%f,%fは元のコードでint型変数に読み込んでいたため、
    // ここでは float 型変数 (targetX_mm, targetY_mm) に読み込むように修正します。
    if ((strncmp(command, "MOVE(", 5) == 0 || strncmp(command, "WARP(", 5) == 0) && command[strlen(command) - 1] == ')')
    {
        float targetX_mm, targetY_mm;
        char *data_start = tempCommand + 5;
        char *data_end = tempCommand + strlen(tempCommand) - 1;

        *data_end = '\0'; // 閉じ括弧をヌル終端に置き換え

        char *x_str = strtok(data_start, ",");
        char *y_str = strtok(NULL, ",");

        if (x_str != NULL && y_str != NULL)
        {
            targetX_mm = atof(x_str);
            targetY_mm = atof(y_str); // ★修正2: atof(y_str) に変更

            Serial.print("WARP (Linear Move) to X:");
            Serial.print(targetX_mm, 2);
            Serial.print(", Y:");
            Serial.println(targetY_mm, 2);

            // X軸とY軸のインデックス
            const int X_INDEX = 1;
            const int Y_INDEX = 2;

            // 1. 目標座標 (mm) を目標ステップ数に変換
            long targetX_steps = (long)(targetX_mm * STEPS_PER_MM_XY);
            long targetY_steps = (long)(targetY_mm * STEPS_PER_MM_XY);

            // 2. 現在位置と移動距離を計算
            long currentX_steps = motors[X_INDEX]->currentPosition();
            long currentY_steps = motors[Y_INDEX]->currentPosition();

            long deltaX = labs(targetX_steps - currentX_steps);
            long deltaY = labs(targetY_steps - currentY_steps);

            // 3. 最も長い移動距離を特定
            long maxDelta = max(deltaX, deltaY);

            if (maxDelta == 0)
            {
                Serial.println("Already at target.");
                return;
            }

            // 4. 各軸の速度を設定し、移動開始
            // 最も長い移動距離を持つ軸 (maxDelta) は最大速度で移動する
            float maxSpeed = motors[X_INDEX]->maxSpeed(); // X, Yは同じMaxSpeed (4000.0)

            // X軸の設定
            motors[X_INDEX]->setSpeed(maxSpeed * ((float)deltaX / maxDelta));
            motors[X_INDEX]->moveTo(targetX_steps);

            // Y軸の設定
            motors[Y_INDEX]->setSpeed(maxSpeed * ((float)deltaY / maxDelta));
            motors[Y_INDEX]->moveTo(targetY_steps);

            return;
        }
    }

    // エラー処理
    Serial.print("Error: Unknown command or invalid format: ");
    Serial.println(command);
}