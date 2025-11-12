#include "robot.hpp"
#include <string.h> // strncmp, strcmp, sscanfのために必要

// コンストラクタ: モーターインスタンスの初期化と配列への格納
Robot::Robot()
    // AccelStepper インスタンスの初期化
    // Z軸 (Uni): FULL4WIRE (ピン順序は元のコードから維持)
    : motorUni(AccelStepper::FULL4WIRE, UNI_PIN1, UNI_PIN3, UNI_PIN4, UNI_PIN2),
      // X軸 (Bip1): DRIVER
      motorBip1(AccelStepper::DRIVER, BIP1_PIN1, BIP1_PIN2),
      // Y軸 (Bip2): DRIVER
      motorBip2(AccelStepper::DRIVER, BIP2_PIN1, BIP2_PIN2)
{
    // モーター配列と軸名の設定
    motors[0] = &motorUni;
    motors[1] = &motorBip1;
    motors[2] = &motorBip2;
    axisNames[0] = "z";
    axisNames[1] = "x";
    axisNames[2] = "y";

    // ★ MultiStepperにX軸とY軸のモーターを登録
    xySteppers.addStepper(motorBip1); // インデックス0: X軸
    xySteppers.addStepper(motorBip2); // インデックス1: Y軸
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
            motors[i]->setMaxSpeed(1200.0);
            motors[i]->setAcceleration(800.0);
            motors[2]->setPinsInverted(true, true, true); // Y軸モータは何故かこの設定。理由は知らない（真顔）
        }
    }

    // キャリブレーションスイッチの設定
    pinMode(XCALIB_PIN, INPUT_PULLUP);
    pinMode(YCALIB_PIN, INPUT_PULLUP);
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

                // 無限に移動するための遠い目標を設定
                long current = motors[motorIndex]->currentPosition();

                // ON+は正の方向、ON-は負の方向に目標を設定
                long target = (command[2] == '+') ? current + 1000000000L : current - 1000000000L;

                motors[motorIndex]->moveTo(target);

                // ★ setSpeed() を使用して、方向を示す符号付きの速度を設定
                float speed = motors[motorIndex]->maxSpeed();
                if (command[2] == '-')
                {
                    speed = -speed; // 逆方向の場合は速度を負にする
                }
                // setSpeed() が呼び出されると、そのモーターは他の協調制御よりもこの速度を優先します
                motors[motorIndex]->setSpeed(speed);

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
                    motor->setSpeed(0); // ★ 追加: 速度を強制的に0に設定
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
                motors[motorIndex]->setSpeed(0); // ★ 追加: 速度を強制的に0に設定
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
        Serial.println("AUTOCALIB START: Moving X and Y axes simultaneously to find limit switches.");

        // 状態を初期化
        isAutoCalibrating = true;

        // Z軸 (motors[0]) はホーミング対象外として最初から完了とする
        axisHomed[0] = true;

        // X軸とY軸を未完了として初期化
        axisHomed[1] = false; // X軸
        axisHomed[2] = false; // Y軸

        // X軸とY軸 (i=1, i=2) のみに対して移動目標を設定
        for (int i = 1; i < NUM_MOTORS; i++)
        {
            // ★ 修正1: 速度と加速度を大幅に上げる
            motors[i]->setMaxSpeed(8000.0);
            motors[i]->setAcceleration(16000.0);

            // ★ 修正2: 負方向への移動開始速度を強制的に設定し、即座に高速移動を開始
            motors[i]->setSpeed(-8000.0); // 負方向への移動なので負の値

            motors[i]->moveTo(-1000000); // 負方向へ移動目標を設定
        }

        shouldWait = true;
        isCommandExecuting = true;
    }
    // 6. HOME (原点復帰/動作) コマンド: X=0, Y=0, Z=0へ移動 (WARP(0,0)と同様に直線補間を使用)
    else if (strcmp(command, "HOME") == 0)
    {
        Serial.println("HOME: Moving to X=0, Y=0, Z=0 (Linear Move / WARP Speed)");

        // Z軸の移動 (単独)
        motors[0]->moveTo(0);

        // X軸とY軸の移動 (MultiStepperで協調移動)
        long xyTargetPositions[2];
        xyTargetPositions[0] = 0; // X軸目標
        xyTargetPositions[1] = 0; // Y軸目標

        // ★ WARPと同じ速度 (100%) を適用
        float standardMaxSpeed = 100.0;
        float standardAcceleration = 600.0;

        float newMaxSpeed = standardMaxSpeed * 2.0;

        // 速度と加速度を標準値 (100%) に設定
        motors[1]->setMaxSpeed(newMaxSpeed);
        motors[2]->setMaxSpeed(newMaxSpeed);
        motors[1]->setAcceleration(standardAcceleration);
        motors[2]->setAcceleration(standardAcceleration);

        // X/Y軸の協調移動開始
        xySteppers.moveTo(xyTargetPositions);

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
        float targetZ_mm = 17.5;
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
    // 9. WARP/MOVE (線形補間移動) コマンド: MultiStepperで直線補間を実現
    else if ((strncmp(command, "MOVE(", 5) == 0 || strncmp(command, "WARP(", 5) == 0) && command[strlen(command) - 1] == ')')
    {
        float targetX_mm, targetY_mm;
        // MOVE(X,Y) または WARP(X,Y) に合わせて data_start を調整
        char *data_start = strncmp(command, "MOVE(", 5) == 0 ? tempCommand + 5 : tempCommand + 5;
        char *data_end = tempCommand + strlen(tempCommand) - 1;

        *data_end = '\0'; // 閉じ括弧をヌル終端に置き換え

        char *x_str = strtok(data_start, ",");
        char *y_str = strtok(NULL, ",");

        if (x_str != NULL && y_str != NULL)
        {
            targetX_mm = atof(x_str) + BOARD_XZERO;
            targetY_mm = atof(y_str) + BOARD_YZERO;

            Serial.print(command[0] == 'M' ? "MOVE" : "WARP");
            Serial.print(" (Linear Move) to X:");
            Serial.print(targetX_mm, 2);
            Serial.print(", Y:");
            Serial.println(targetY_mm, 2);

            const int X_INDEX = 1;
            const int Y_INDEX = 2;

            long targetX_steps = (long)(targetX_mm * STEPS_PER_MM_X);
            long targetY_steps = (long)(targetY_mm * STEPS_PER_MM_Y);

            // ★ MultiStepper用の目標位置配列
            long xyTargetPositions[2];
            xyTargetPositions[0] = targetX_steps; // MultiStepperのインデックス0 = X軸
            xyTargetPositions[1] = targetY_steps; // MultiStepperのインデックス1 = Y軸

            long currentX_steps = motors[X_INDEX]->currentPosition();
            long currentY_steps = motors[Y_INDEX]->currentPosition();

            // 目標位置に既にいるかチェック
            if (currentX_steps == targetX_steps && currentY_steps == targetY_steps)
            {
                Serial.println("Already at target.");
            }
            else
            {
                // X/Y軸の標準設定速度を取得
                // WARP速度を100%として、MOVE速度を50%にする
                float standardMaxSpeed = 100.0; // motors[1]->maxSpeed() で取得しても良い
                float speedFactor = 1.0;

                if (strncmp(command, "MOVE(", 5) == 0)
                {
                    speedFactor = 3.0; // MOVE: 80%
                }
                else // WARP
                {
                    speedFactor = 5.0; // WARP: 200%
                }

                float newMaxSpeed = standardMaxSpeed * speedFactor;
                float newAcceleration = motors[X_INDEX]->acceleration() * speedFactor; // 加速度も速度に合わせて調整

                // ★ 速度と加速度を一時的に変更
                motors[X_INDEX]->setMaxSpeed(newMaxSpeed);
                motors[Y_INDEX]->setMaxSpeed(newMaxSpeed);
                motors[X_INDEX]->setAcceleration(newAcceleration);
                motors[Y_INDEX]->setAcceleration(newAcceleration);

                // MultiStepperの moveTo を呼び出す (新しい設定速度で動作する)
                xySteppers.moveTo(xyTargetPositions);

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
    // run() が MultiStepper によって設定された協調ステップを実行する

    // Z軸 (motors[0]) は単独で駆動
    motors[0]->run();

    // X軸とY軸は MultiStepper で協調駆動
    // ★ MultiStepper::run() を呼び出すことで、XとYが同時に目標に到達するようにステップが調整される
    xySteppers.run();
    // ★ AUTOCALIBの非同期ホーミング監視 (修正後)
    if (isAutoCalibrating)
    {
        bool allHomed = true;

        for (int i = 1; i < NUM_MOTORS; i++) // X軸 (i=1) と Y軸 (i=2) のみループ
        {
            if (!axisHomed[i]) // まだホーミングが完了していない軸をチェック
            {
                int pinIndex = i - 1;

                // フェーズ1: スイッチ探索中 (ターゲットが負の大きな値)
                // targetPosition() < 0 で、まだリバウンド移動を開始していない軸を判定
                if (motors[i]->targetPosition() < 0)
                {
                    if (digitalRead(axisPins[pinIndex]) == LOW) // 接触を検出！
                    {
                        motors[i]->stop();

                        // ① 接触位置を負の REBOUND_STEPS に設定
                        // これにより、目標位置 0 がリバウンド後の原点となる (ズレ防止)
                        motors[i]->setCurrentPosition(-REBOUND_STEPS);

                        // リバウンド移動の速度と加速度を設定 (安全のため低速に)
                        motors[i]->setMaxSpeed(500.0);
                        motors[i]->setAcceleration(1000.0);

                        // ② 正方向へリバウンド移動を開始 (目標位置 0 まで移動)
                        motors[i]->moveTo(0);

                        Serial.print(axisNames[i]);
                        Serial.println(" axis HOMED (Started REBOUND move to 0).");
                    }
                }

                // フェーズ2: リバウンド移動中/完了 (ターゲットが 0)
                else if (motors[i]->targetPosition() == 0)
                {
                    if (motors[i]->distanceToGo() == 0) // リバウンド完了
                    {
                        // リバウンド完了！ currentPosition() は 0 になっている。
                        axisHomed[i] = true; // 最終的な完了フラグを立てる
                        Serial.print(axisNames[i]);
                        Serial.println(" axis REBOUND finished. Position confirmed at 0.");
                    }
                }

                if (!axisHomed[i])
                {
                    allHomed = false; // XまたはYがまだ完了していない
                }
            }
        }

        // 全ての軸がホーミング完了したかチェック
        if (allHomed)
        {
            Serial.println("AUTOCALIB finished successfully. Restoring motor speed.");

            // ★ 速度を元の設定に戻す (元の設定値: 200.0 / 800.0)
            motors[1]->setMaxSpeed(200.0);
            motors[2]->setMaxSpeed(200.0);
            motors[1]->setAcceleration(800.0);
            motors[2]->setAcceleration(800.0);

            // ... (後処理) ...
            isAutoCalibrating = false;
            isCommandExecuting = false;
            queueTail = (queueTail + 1) % MAX_COMMANDS;
        }

        return; // AUTOCALIBが実行中は、その下の通常の終了判定ロジックをスキップ
    }
    if (isCommandExecuting)
    {

        // X, Y, Z軸全ての移動が完了したかチェック
        if (motors[0]->distanceToGo() == 0 &&
            motors[1]->distanceToGo() == 0 &&
            motors[2]->distanceToGo() == 0)
        {
            // 実行完了
            Serial.print("Command finished: ");
            Serial.println(commandQueue[queueTail].commandString);

            // 座標を再確認 (オプション)
            printCoordinates();

            // ★ X/Y軸の速度/加速度を元の設定に戻す (MOVE/WARPで変更した場合のクリーンアップ)
            motors[1]->setMaxSpeed(200.0);     // X軸の元の設定
            motors[2]->setMaxSpeed(200.0);     // Y軸の元の設定
            motors[1]->setAcceleration(800.0); // X軸の元の設定
            motors[2]->setAcceleration(800.0); // Y軸の元の設定

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