// 受信バッファの最大サイズを定義
const int maxCommandLength = 32;
char serialBuffer[maxCommandLength];
int bufferIndex = 0;

void setup()
{
  // シリアル通信の初期化
  Serial.begin(9600);
  Serial.println("Ready to read command");
}

void loop()
{
  // シリアルデータが利用可能かチェック
  while (Serial.available())
  {
    char incomingChar = Serial.read();

    // 改行文字またはコマンドの終端を検出
    if (incomingChar == '\n' || incomingChar == '\r')
    {
      // 受信バッファをヌル終端
      serialBuffer[bufferIndex] = '\0';

      // 受信したコマンドを処理
      processCommand(serialBuffer);

      // バッファをリセット
      bufferIndex = 0;
      memset(serialBuffer, 0, maxCommandLength); // バッファクリア（念のため）
    }
    else if (bufferIndex < maxCommandLength - 1)
    {
      // バッファに文字を追加
      serialBuffer[bufferIndex] = incomingChar;
      bufferIndex++;
    }
    // else: コマンドが長すぎる場合は無視
  }
}

// コマンド処理関数
void processCommand(const char *command)
{
  if (strcmp(command, "HOME") == 0)
  {
    Serial.println("Let's go home");
    return;
  }

  if (strncmp(command, "MOVE(", 5) == 0 && command[strlen(command) - 1] == ')')
  {
    int x = 0;
    int y = 0;

    if (sscanf(command + 5, "%d,%d)", &x, &y) == 2)
    {
      Srial.print("Let's move to ")
          Serial.print(x);
      Serial.print(",");
      Serial.println(y);
      return;
    }
  }

  if (strcmp(command, "UP") == 0)
  {
    Serial.println("Let's raise z-axis");
    return;
  }

  if (strcmp(command, "DOWN") == 0)
  {
    Serial.println("Let's drop z-axis");
    return;
  }

  if (strcmp(command, "CALIB") == 0)
  {
    Serial.println("Let's calibrate all axis");
    return;
  }

  // どのコマンドにもマッチしない場合
  Serial.print("Error: Unknown command or invalid format: ");
  Serial.println(command);
}