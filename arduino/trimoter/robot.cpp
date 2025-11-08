#include "robot.hpp"
#include <string.h> // strncmp, strcmp, sscanfのために必要

// コンストラクタ: モーターインスタンスの初期化と配列への格納
Robot::Robot()
    // AccelStepper インスタンスの初期化
    // Z軸 (Uni): FULL4WIRE (ピン順序は元のコードから維持)
    : motorUni(AccelStepper::FULL4WIRE, UNI_PIN1, UNI_PIN3, UNI_PIN4, UNI_PIN2),
      // X軸 (Bip1): FULL4WIRE (ピン3,4は0)
      motorBip1(AccelStepper::DRIVER, BIP1_PIN1, BIP1_PIN2),
      // Y軸 (Bip2): FULL4WIRE (ピン3,4は0)
      motorBip2(AccelStepper::DRIVER, BIP2_PIN1, BIP2_PIN2)
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

    // モーターの設定

    for (int i = 0; i < NUM_MOTORS; i++)
    {
        if (i == 0) // Z軸 (motorUni)
        {
            motors[i]->setMaxSpeed(500.0);
            motors[i]->setAcceleration(10000.0);
        }
        else // X, Y軸 (motorBip1, motorBip2)
        {
            motors[i]->setMaxSpeed(100.0);
            motors[i]->setAcceleration(600.0);
            motors[2]->setPinsInverted(true, true, true); // Y軸モータは何故かこの設定。理由は知らない（真顔）
        }
    }

    // キャリブレーションスイッチの設定
    pinMode(XCALIB_PIN, INPUT_PULLUP);
    pinMode(YCALIB_PIN, INPUT_PULLUP);
    pinMode(ZCALIB_PIN, INPUT_PULLUP);
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
    float x_coord = (float)x_steps / STEPS_PER_MM_X;
    float y_coord = (float)y_steps / STEPS_PER_MM_Y;

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
    float x_coord = (float)x_steps / STEPS_PER_MM_X;
    float y_coord = (float)y_steps / STEPS_PER_MM_Y;

    // シリアルに出力
    Serial.print("COORD Z:");
    Serial.print(z_coord, 2);
    Serial.print(", X:");
    Serial.print(x_coord, 2);
    Serial.print(", Y:");
    Serial.println(y_coord, 2);
}

void Robot::processCommand(const char *command)
{

    char axis = ' ';
    int targetPos = 0;
    int motorIndex = -1;

    char tempCommand[32];
    strncpy(tempCommand, command, 31);
    tempCommand[31] = '\0';

    // 1. ON+/ON-/OFF コマンド (即時実行)
    // ON+, ON-, OFF はリアルタイムな操作（緊急停止など）として即時実行する
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
                // motors[motorIndex]->setSpeed(motors[motorIndex]->maxSpeed());
                Serial.println((command[2] == '+') ? "+" : "-");
                return;
            }
        }
    }

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

    // 2. それ以外のコマンド (CALIB, HOME, COORD, MOVE/WARP, UP, DOWN) はキューに追加
    // CALIB/HOME/COORD/MOVE/WARP/UP/DOWN の処理は executeNextCommand() に移動
    enqueueCommand(command);
}

bool Robot::enqueueCommand(const char *command)
{
    // キューがいっぱいかチェック (一つ空けておくことで head == tail の判定を「空」に統一)
    if ((queueHead + 1) % MAX_COMMANDS == queueTail)
    {
        Serial.println("Error: Command queue is full. Command rejected.");
        return false;
    }

    // コマンドをコピーしてキューに格納
    strncpy(commandQueue[queueHead].commandString, command, 31);
    commandQueue[queueHead].commandString[31] = '\0';
    commandQueue[queueHead].isExecuting = false;

    // head を進める
    queueHead = (queueHead + 1) % MAX_COMMANDS;
    Serial.print("Command queued: ");
    Serial.println(command);
    return true;
}
// robot.cpp (修正後の executeNextCommand)

void Robot::executeNextCommand()
{
    // キューが空なら何もしない
    if (queueHead == queueTail)
    {
        return;
    }

    // 次のコマンドを取得 (tailが指す位置)
    const char *command = commandQueue[queueTail].commandString;

    // ★ 実行開始ログ
    Serial.print("Executing: ");
    Serial.println(command);

    char tempCommand[32];
    strncpy(tempCommand, command, 31);
    tempCommand[31] = '\0';

    // 移動を伴い、完了を待つ必要があるかどうかを判定するフラグ
    bool shouldWait = false;

    // 4. CALIB (キャリブレーション/初期化) コマンド: 全モーターの位置を0にリセット
    if (strcmp(command, "CALIB") == 0)
    {
        Serial.println("CALIB: All Motor Positions Reset (0 steps)");
        for (int i = 0; i < NUM_MOTORS; i++)
        {
            motors[i]->setCurrentPosition(0);
        }
        // shouldWait は false (即時完了) のまま
    }

    // 5. AUTOCALIB (自動ホーミング) コマンド: 非同期でホーミングを開始
    else if (strcmp(command, "AUTOCALIB") == 0)
    {
        Serial.println("AUTOCALIB START: Moving all axes simultaneously to find limit switches.");

        // 状態を初期化
        isAutoCalibrating = true;
        axisHomed[0] = axisHomed[1] = axisHomed[2] = false;

        // 全軸に対して負方向へ遠くに移動する目標を設定（同時に移動開始）
        for (int i = 0; i < NUM_MOTORS; i++)
        {
            motors[i]->moveTo(-1000000);
            // 速度調整が必要な場合はここで行います
        }

        shouldWait = true;         // 完了を runMotors() で監視する
        isCommandExecuting = true; // モーター駆動を伴うコマンドとしてマーク
    }

    // 6. HOME (原点復帰/動作) コマンド: X=0, Y=0, Z=0へ移動
    else if (strcmp(command, "HOME") == 0)
    {
        Serial.println("HOME: Moving to X=0, Y=0, Z=0");
        motors[1]->moveTo(0); // X軸 (motorBip1) を絶対位置0へ移動
        motors[2]->moveTo(0); // Y軸 (motorBip2) を絶対位置0へ移動
        motors[0]->moveTo(0); // Z軸 (motorUni) を絶対位置0へ移動
        shouldWait = true;
    }

    // 7. COORD (座標確認) コマンド
    else if (strcmp(command, "COORD") == 0)
    {
        float current_coords[3];
        getCoordinates(current_coords);

        Serial.print("COORD X:");
        Serial.print(current_coords[0], 2);
        Serial.print(", Y:");
        Serial.print(current_coords[1], 2);
        Serial.print(", Z:");
        Serial.println(current_coords[2], 2);
        // shouldWait は false (即時完了) のまま
    }

    // 8. UP/DOWN コマンド (Z軸移動)
    else if (strcmp(command, "UP") == 0)
    {
        Serial.println("Z-Magnet UP: Move to 10mm");
        float targetZ_mm = 15;
        long targetZ_steps = (long)(targetZ_mm * STEPS_PER_MM_Z);
        motors[0]->moveTo(targetZ_steps); // Z軸 (motorUni) を絶対位置 100mmへ移動
        shouldWait = true;
    }

    else if (strcmp(command, "DOWN") == 0)
    {
        Serial.println("Z-Magnet DOWN: Move to -10mm");

        float targetZ_mm = 0;
        long targetZ_steps = (long)(targetZ_mm * STEPS_PER_MM_Z);
        motors[0]->moveTo(targetZ_steps); // Z軸 (motorUni) を絶対位置 -100mmへ移動
        shouldWait = true;
    }

    // 9. WARP/MOVE (線形補間移動) コマンド
    else if (strncmp(command, "MOVE(", 5) == 0 && command[strlen(command) - 1] == ')')
    {
        // ... (MOVE/WARPの複雑なパースと移動開始ロジックをここに移植) ...
        // 既存の MOVE/WARP ロジックのコピーを貼り付け、最後の 'return;' を削除

        float targetX_mm, targetY_mm;
        char *data_start = tempCommand + 5;
        char *data_end = tempCommand + strlen(tempCommand) - 1;

        *data_end = '\0'; // 閉じ括弧をヌル終端に置き換え

        char *x_str = strtok(data_start, ",");
        char *y_str = strtok(NULL, ",");

        if (x_str != NULL && y_str != NULL)
        {
            targetX_mm = atof(x_str);
            targetY_mm = atof(y_str);

            Serial.print("MOVE (Linear Move) to X:");
            Serial.print(targetX_mm, 2);
            Serial.print(", Y:");
            Serial.println(targetY_mm, 2);

            const int X_INDEX = 1;
            const int Y_INDEX = 2;

            long targetX_steps = (long)(targetX_mm * STEPS_PER_MM_X);
            long targetY_steps = (long)(targetY_mm * STEPS_PER_MM_Y);

            long currentX_steps = motors[X_INDEX]->currentPosition();
            long currentY_steps = motors[Y_INDEX]->currentPosition();

            long deltaX = labs(targetX_steps - currentX_steps);
            long deltaY = labs(targetY_steps - currentY_steps);

            long maxDelta = max(deltaX, deltaY);

            if (maxDelta == 0)
            {
                Serial.println("Already at target.");
                // shouldWait は false (即時完了) のまま
            }
            else
            {
                float maxSpeed = motors[X_INDEX]->maxSpeed() / 10;

                // motors[X_INDEX]->setSpeed(maxSpeed * ((float)deltaX / maxDelta));
                // motors[X_INDEX]->setSpeed(0);
                motors[X_INDEX]->moveTo(targetX_steps);

                // motors[Y_INDEX]->setSpeed(maxSpeed * ((float)deltaY / maxDelta));
                // motors[X_INDEX]->setSpeed(0);
                motors[Y_INDEX]->moveTo(targetY_steps);

                shouldWait = true;
            }
        }
    }

    // 10. WARP/MOVE (線形補間移動) コマンド
    else if (strncmp(command, "WARP(", 5) == 0 && command[strlen(command) - 1] == ')')
    {
        // ... (MOVE/WARPの複雑なパースと移動開始ロジックをここに移植) ...
        // 既存の MOVE/WARP ロジックのコピーを貼り付け、最後の 'return;' を削除

        float targetX_mm, targetY_mm;
        char *data_start = tempCommand + 5;
        char *data_end = tempCommand + strlen(tempCommand) - 1;

        *data_end = '\0'; // 閉じ括弧をヌル終端に置き換え

        char *x_str = strtok(data_start, ",");
        char *y_str = strtok(NULL, ",");

        if (x_str != NULL && y_str != NULL)
        {
            targetX_mm = atof(x_str);
            targetY_mm = atof(y_str);

            Serial.print("WARP (Linear Move) to X:");
            Serial.print(targetX_mm, 2);
            Serial.print(", Y:");
            Serial.println(targetY_mm, 2);

            const int X_INDEX = 1;
            const int Y_INDEX = 2;

            long targetX_steps = (long)(targetX_mm * STEPS_PER_MM_X);
            long targetY_steps = (long)(targetY_mm * STEPS_PER_MM_Y);

            long currentX_steps = motors[X_INDEX]->currentPosition();
            long currentY_steps = motors[Y_INDEX]->currentPosition();

            long deltaX = labs(targetX_steps - currentX_steps);
            long deltaY = labs(targetY_steps - currentY_steps);

            long maxDelta = max(deltaX, deltaY);

            if (maxDelta == 0)
            {
                Serial.println("Already at target.");
                // shouldWait は false (即時完了) のまま
            }
            else
            {
                float maxSpeed = motors[X_INDEX]->maxSpeed();

                // motors[X_INDEX]->setSpeed(maxSpeed * ((float)deltaX / maxDelta));
                // motors[X_INDEX]->setSpeed(0);
                motors[X_INDEX]->moveTo(targetX_steps);

                // motors[Y_INDEX]->setSpeed(maxSpeed * ((float)deltaY / maxDelta));
                // motors[X_INDEX]->setSpeed(0);
                motors[Y_INDEX]->moveTo(targetY_steps);

                shouldWait = true;
            }
        }
    }
    else
    {
        Serial.print("Error: Unknown command or invalid format: ");
        Serial.println(command);
        // 不明なコマンドも即時完了と見なす
    }

    // 実行を伴うコマンドの場合、フラグを立てる (関数末尾でまとめて処理)
    if (shouldWait)
    {
        isCommandExecuting = true;
        // isCommandExecuting が true の場合、runMotors() が完了を監視し、
        // 完了後に queueTail を進める
    }
    else
    {
        // CALIB, COORD, エラーコマンドなど、移動を伴わないコマンドは即時完了と見なす
        queueTail = (queueTail + 1) % MAX_COMMANDS;
        Serial.println("Command completed immediately.");
    }
}

void Robot::runMotors()
{
    // 1. 全てのモーターを駆動
    for (int i = 0; i < NUM_MOTORS; i++)
    {
        motors[i]->run();
    }

    // ★ 新規追加: AUTOCALIBの非同期ホーミング監視
    if (isAutoCalibrating)
    {
        bool allHomed = true;

        for (int i = 0; i < NUM_MOTORS; i++)
        {
            if (!axisHomed[i]) // まだホーミングが完了していない軸をチェック
            {
                // スイッチが押されたかチェック (LOWで接触と仮定)
                if (digitalRead(axisPins[i]) == LOW)
                {
                    // 接触を検出！
                    motors[i]->stop();                // 減速停止
                    motors[i]->setCurrentPosition(0); // 原点位置に設定

                    Serial.print(axisNames[i]);
                    Serial.println(" axis HOMED and position set to 0.");

                    axisHomed[i] = true; // 完了フラグを立てる
                }
                else
                {
                    allHomed = false; // まだ完了していない軸がある
                }
            }
        }

        // 全ての軸がホーミング完了したかチェック
        if (allHomed)
        {
            Serial.println("AUTOCALIB finished successfully.");

            // AUTOCALIBの状態をリセット
            isAutoCalibrating = false;

            // 実行状態をリセットし、キューの先頭を進める
            isCommandExecuting = false;
            queueTail = (queueTail + 1) % MAX_COMMANDS;
        }

        return; // AUTOCALIBが実行中は、その下の通常の終了判定ロジックをスキップ
    }

    // 2. 実行中のキューコマンドの終了判定
    if (isCommandExecuting)
    {

        // X, Y, Z軸全ての移動が完了したかチェック
        // run() が呼び出されることで motors[i]->distanceToGo() が 0 になる
        if (motors[0]->distanceToGo() == 0 &&
            motors[1]->distanceToGo() == 0 &&
            motors[2]->distanceToGo() == 0)
        {
            // 実行完了
            Serial.print("Command finished: ");
            Serial.println(commandQueue[queueTail].commandString);

            // 座標を再確認 (オプション)
            printCoordinates();

            // 実行状態をリセット
            isCommandExecuting = false;

            // キューの先頭 (tail) を進める
            queueTail = (queueTail + 1) % MAX_COMMANDS;
        }
    }

    // 3. 次のコマンドのディスパッチ
    // 実行中のコマンドがない (isCommandExecuting == false) かつ
    // キューに未実行のコマンドがある (queueHead != queueTail) 場合に実行開始
    if (!isCommandExecuting && queueHead != queueTail)
    {
        executeNextCommand();
    }
}
