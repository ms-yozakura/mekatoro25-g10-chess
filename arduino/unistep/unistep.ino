

/*
    This test code is for driving stepper moter (unipole) from laptop with simple code

    Arduino recieves combination of code and parses it to a gorup of single code
    (e.g. ON/ OFF/ MOVE()/ HOME ...),
    then the codes and arguments will be used for driving moters.

    in this code, ON/OFF　are used for moter control, but HOME and MOVE are just for dummy.
    They return Serial print to laptop.
*/
int MYPIN1 = 4;
int MYPIN2 = 5;
int MYPIN3 = 6;
int MYPIN4 = 7;

int STEP_DELAY = 2;

int motorPins[4] = {MYPIN1, MYPIN2, MYPIN3, MYPIN4};

const int maxCommandLength = 32;
char serialBuffer[maxCommandLength];
int bufferIndex = 0;

int motor_move = 0;
int motor_step = 0;

void setup()
{
  for (int i = 0; i < 4; i++)
    pinMode(motorPins[i], OUTPUT);
  Serial.begin(9600);
  Serial.println("Ready to receive commands (ON/OFF/MOVE(x,y)/HOME)");
}

void loop()
{
  if (motor_move == 1)
  {
    motor_step = (motor_step + 1) % 4;

    int seq[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1}};

    for (int i = 0; i < 4; i++)
    {
      digitalWrite(motorPins[i], seq[motor_step][i]);
    }
    delay(STEP_DELAY);
  }
  if (motor_move == -1)
  {
    motor_step = (motor_step - 1 + 4) % 4;

    int seq[4][4] = {
        {1, 0, 0, 0},
        {0, 1, 0, 0},
        {0, 0, 1, 0},
        {0, 0, 0, 1}};

    for (int i = 0; i < 4; i++)
    {
      digitalWrite(motorPins[i], seq[motor_step][i]);
    }
    delay(STEP_DELAY);
  }

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

void processCommand(const char *command)
{
  if (strncmp(command, "ON+(", 4) == 0 && command[strlen(command) - 1] == ')')
  {
    char axis;
    if (sscanf(command + 4, "%c)", &axis) == 2)
      Serial.println(axis + "-Motor ON+");
    motor_move = 1;
    return;
  }

  if (strncmp(command, "ON-(", 4) == 0 && command[strlen(command) - 1] == ')')
  {
    char axis;
    if (sscanf(command + 4, "%c)", &axis) == 1)
      Serial.println(axis + "-Motor ON-");
    motor_move = -1;
    return;
  }

  if (strncmp(command, "OFF(", 4) == 0 && command[strlen(command) - 1] == ')')
  {
    char axis;
    if (sscanf(command + 4, "%c)", &axis) == 1)
      Serial.println(axis + "-Motor OFF");
    motor_move = 0;
    return;
  }

  if (strcmp(command, "UP") == 0)
  {
    Serial.println("Z-Magnet UP: ON+z equivalent");
    // 実際にモーターを動かす場合は、ここで ON+z と MOVE を組み合わせたZ軸移動処理を呼び出す
    return;
  }

  if (strcmp(command, "DOWN") == 0)
  {
    Serial.println("Z-Magnet DOWN: OFFz equivalent");
    // 実際にモーターを動かす場合は、ここで ON-z と MOVE を組み合わせたZ軸移動処理を呼び出す
    return;
  }

  if (strncmp(command, "WARP(", 5) == 0 && command[strlen(command) - 1] == ')')
  {
    int x, y;
    // MOVEとWARPのフォーマットは同じであるため、MOVEと同じ処理ロジックを利用
    if (sscanf(command + 5, "%f,%f)", &x, &y) == 2)
    { // 座標がfloat型で来ている可能性があるため、sscanfのフォーマットを調整する必要があるかもしれませんが、ここでは一旦intで受けます。
      Serial.print("WARP to ");
      Serial.print(x);
      Serial.print(",");
      Serial.println(y);
      return;
    }
  }

  if (strcmp(command, "HOME") == 0)
  {
    Serial.println("0,0");
    return;
  }

  if (strncmp(command, "MOVE(", 5) == 0 && command[strlen(command) - 1] == ')')
  {
    int x, y;
    if (sscanf(command + 5, "%f,%f)", &x, &y) == 2)
    {
      Serial.print("MOVE to ");
      Serial.print(x);
      Serial.print(",");
      Serial.println(y);
      return;
    }
  }

  Serial.print("Error: Unknown command or invalid format: ");
  Serial.println(command);
}