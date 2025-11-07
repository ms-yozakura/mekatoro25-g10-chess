// mainwindow.cpp

#include "main_window.hpp"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include "chess/chess_game.hpp"          // ChessGame
#include "widget/chess_board_widget.hpp" // ChessBoardWidget
// シリアル通信関数を使うため、serial.hpp が必要（元のコードに合わせて暫定的に含めます）
#include "serial.hpp"
#include "serial_manager.hpp" // SerialManager の定義をインクルード
#include <QFile>

// コンストラクタ
MainWindow::MainWindow(ChessGame *game, SerialManager *serialManager, QWidget *parent)
    // m_fd の代わりに m_serialManager を初期化
    : QMainWindow(parent), m_game(game), m_serialManager(serialManager), m_turnWhite(true)
{
    setWindowTitle("Qt Serial Motor Control (Refactored)");
    // ... (setupUI, setupConnections, 初期FENの呼び出しはそのまま)
    setupUI();
    setupConnections();
    m_fenInput->textChanged(m_fenInput->text());
}

// UI要素の構築とスタイルの設定
void MainWindow::setupUI()
{
    // 中央ウィジェット
    QWidget *central = new QWidget;
    setCentralWidget(central);

    // スタイルシートファイルのパスを指定
    QFile file("./css/style.css");
    // ファイルを開く
    if (file.open(QFile::ReadOnly | QFile::Text))
    {
        // ストリームを作成し、ファイルの内容を全て読み込む
        QTextStream stream(&file);
        QString styleSheet = stream.readAll();

        // ファイルを閉じる
        file.close();

        // プリケーション全体にスタイルシートを適用
        central->setStyleSheet(styleSheet);
    }

    // UI要素の初期化
    // 盤面
    m_boardWidget = new ChessBoardWidget;
    // タイトル
    m_label = new QLabel("Automatic Chess Board");
    m_label->setAlignment(Qt::AlignCenter);
    // FEN入力のためのテキストボックス
    m_fenInput = new QLineEdit;
    m_fenInput->setPlaceholderText("FEN形式の文字列...");
    m_fenInput->setObjectName("fenInputBox");
    // 次の手入力のためのテキストボックス
    m_moveInput = new QLineEdit;
    m_moveInput->setPlaceholderText("e.g. e2e4/b1c3 ...");
    m_moveInput->setObjectName("fenInputBox");
    m_moveInputButton = new QPushButton("決定");
    // 合法手/最適手/Arduinoコマンド表示領域
    m_moveListLabel = new QLabel("Legal Moves: N/A");
    m_moveListLabel->setWordWrap(true);
    m_bestMoveLabel = new QLabel("Best Move: N/A");
    m_bestMoveLabel->setWordWrap(true);
    m_commandLabel = new QLabel("Command for Arduino: N/A");
    m_commandLabel->setWordWrap(true);
    // 制御ボタンたち
    m_xmoter_button_p = new QPushButton("X軸 ＋");
    m_xmoter_button_n = new QPushButton("X軸 ー");
    m_ymoter_button_p = new QPushButton("Y軸 ＋");
    m_ymoter_button_n = new QPushButton("Y軸 ー");
    m_zmoter_button_p = new QPushButton("Z軸 ＋");
    m_zmoter_button_n = new QPushButton("Z軸 ー");
    m_moter_button_off = new QPushButton("モーター OFF");
    m_calib_button = new QPushButton("キャリブレーション");

    // 初期FEN設定 (元の main.cpp から移動)
    QString startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
    m_fenInput->setText(startFEN);

    // レイアウトの設定
    QHBoxLayout *layout1 = new QHBoxLayout;
    layout1->setContentsMargins(20, 20, 20, 20);
    layout1->setSpacing(15);
    QVBoxLayout *layout2 = new QVBoxLayout;
    layout2->setContentsMargins(20, 20, 20, 20);
    layout2->setSpacing(15);

    QHBoxLayout *x_buttonlayout = new QHBoxLayout;

    QHBoxLayout *y_buttonlayout = new QHBoxLayout;

    QHBoxLayout *z_buttonlayout = new QHBoxLayout;

    QHBoxLayout *moveInputlayout = new QHBoxLayout;

    x_buttonlayout->addWidget(m_xmoter_button_p);
    x_buttonlayout->addWidget(m_xmoter_button_n);
    y_buttonlayout->addWidget(m_ymoter_button_p);
    y_buttonlayout->addWidget(m_ymoter_button_n);
    z_buttonlayout->addWidget(m_zmoter_button_p);
    z_buttonlayout->addWidget(m_zmoter_button_n);

    moveInputlayout->addWidget(m_moveInput);
    moveInputlayout->addWidget(m_moveInputButton);

    layout2->addWidget(m_label);
    layout2->addWidget(m_fenInput);
    layout2->addWidget(m_moveListLabel);
    layout2->addWidget(m_bestMoveLabel);
    layout2->addWidget(m_commandLabel);
    layout2->addLayout(moveInputlayout);
    layout2->addWidget(m_calib_button);
    layout2->addLayout(x_buttonlayout);
    layout2->addLayout(y_buttonlayout);
    layout2->addLayout(z_buttonlayout);
    layout2->addWidget(m_moter_button_off);

    layout1->addWidget(m_boardWidget);
    layout1->addLayout(layout2);

    central->setLayout(layout1);
}

// シグナルとスロットの接続
void MainWindow::setupConnections()
{
    // モーター制御ボタン (スロットはそのまま)
    connect(m_xmoter_button_p, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_p_clicked('x'); });
    connect(m_xmoter_button_n, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_n_clicked('x'); });
    connect(m_ymoter_button_p, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_p_clicked('y'); });
    connect(m_ymoter_button_n, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_n_clicked('y'); });
    connect(m_zmoter_button_p, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_p_clicked('z'); });
    connect(m_zmoter_button_n, &QPushButton::clicked, this, [this]()
            { this->on_moter_button_n_clicked('z'); });
    connect(m_moveInputButton, &QPushButton::clicked, this, [this]()
            { this->on_moveinput_button_clicked(); });

    connect(m_moter_button_off, &QPushButton::clicked, this, &MainWindow::on_moter_button_off_clicked);
    connect(m_calib_button, &QPushButton::clicked, this, &MainWindow::on_calib_button_clicked);

    // FEN入力 (そのまま)
    connect(m_fenInput, &QLineEdit::textChanged, this, &MainWindow::on_fenInput_textChanged);
    // ★★★ SerialManager からのシグナルを MainWindow のスロットに接続 ★★★
    connect(m_serialManager, &SerialManager::serialError, this, &MainWindow::handleSerialError);
    connect(m_serialManager, &SerialManager::motorStatusChanged, this, &MainWindow::handleMotorStatusChanged);

    // ★★★ 追加: AIのコマンド送信シグナルをワーカースレッドのprocessLongCommandSlotに接続 ★★★
    // m_serialManagerは別スレッドにいるため、この接続でスロットはワーカースレッドで実行される
    connect(this, &MainWindow::sendAiCommand,
            m_serialManager, &SerialManager::processLongCommandSlot);
}

// --- スロットの実装（イベントハンドラ）---
void MainWindow::on_moter_button_p_clicked(char axis)
{
    if (!m_serialManager->isPortOpen())
    {
        QMessageBox::warning(this, "Warning", "Serial port is not opened. Cannot operate this action.");
        return;
    }

    if (m_serialManager->isMotorOn(axis))
    {
        m_serialManager->sendOff(axis);
    }
    else
    {
        m_serialManager->sendOnPlus(axis);
    }
}
void MainWindow::on_moter_button_n_clicked(char axis)
{
    if (!m_serialManager->isPortOpen())
    {
        QMessageBox::warning(this, "Warning", "Serial port is not opened. Cannot operate this action.");
        return;
    }

    if (m_serialManager->isMotorOn(axis))
    {
        m_serialManager->sendOff(axis);
    }
    else
    {
        m_serialManager->sendOnMinus(axis);
    }
}

void MainWindow::on_moter_button_off_clicked()
{
    if (!m_serialManager->isPortOpen())
    {
        QMessageBox::warning(this, "Warning", "Serial port is not opened. Cannot operate this action.");
        return;
    }

    m_serialManager->sendOff('a');
}

void MainWindow::on_calib_button_clicked()
{
    if (!m_serialManager->isPortOpen())
    {
        QMessageBox::warning(this, "Warning", "Serial port is not opened. Cannot operate this action.");
        return;
    }

    m_serialManager->sendCalib();
}

void MainWindow::on_moveinput_button_clicked()
{
    const QString &text = m_fenInput->text();
    const QString &movetext = m_moveInput->text();

    std::string newBoard[8];
    QStringList ranks = text.split('/');

    // FEN形式ではないと判断した場合は処理を中断
    if (ranks.size() != 8)
        return;

    for (int i = 0; i < 8; ++i)
    {
        QString rank = ranks[i];
        std::string row_str = "";

        for (QChar pieceChar : rank)
        {
            if (pieceChar.isDigit())
            {
                int emptyCount = pieceChar.digitValue();
                for (int k = 0; k < emptyCount; ++k)
                {
                    row_str += '*';
                }
            }
            else
            {
                row_str += pieceChar.toLatin1();
            }
        }
        newBoard[i] = row_str;
    }

    m_boardWidget->setBoardState(newBoard);

    // 2. ChessGameに状態を設定
    m_game->initBoardWithStrings(newBoard);
    // bool turnWhite = true; // 仮に白番として処理 (FENから読み取るべき情報だが、元のコードに合わせて暫定的に固定)

    Move thismove;
    std::string moveAlg = movetext.toStdString();
    if (m_game->algebraicToMove(moveAlg, thismove))
    {
        // qDebug() << "有効な移動を検出: " << movetext;
        m_moveInput->clear();
    }
    else
    {
        // qDebug() << "無効な移動形式: " << movetext;
    }

    // 実際にプレイヤーの手をうつ
    // if (m_game->isLegal(thismove, m_turnWhite))
    // {
    //     m_game->makeMove(thismove);
    //     m_turnWhite = false;
    // }

    if (m_game->isLegal(thismove, m_turnWhite))
    {
        m_game->makeMove(thismove);
        m_turnWhite = false; // 白から黒（AI）へ

        // // ユーザーの手を打った後の盤面を取得し、FENを更新
        // // ... myfen を構築 ...
        // m_fenInput->setText(QString::fromStdString(myfen));
        // m_boardWidget->setBoardState(nowRows); // ユーザーの指し手後の盤面を描画

        // // ★★★★ AIの手番を独立した関数として実行 ★★★★
        // // QTimer::singleShotなどを使って非同期に実行する方が望ましいが、一旦同期で。
        // // FEN変更後のAIのターン処理を呼び出す
        // processAITurn();
    }

    std::string nowRows[8];
    m_game->getBoardAsStrings(nowRows);

    std::string myfen = "";

    for (int i = 0; i < 8; ++i)
    {
        // 最初の要素 (i == 0) の場合は区切り文字を追加しない
        if (i > 0)
        {
            myfen += "/"; // i > 0 の場合に区切り文字を追加
        }

        // 現在の行の文字列を追加
        myfen += nowRows[i];
    }

    m_fenInput->setText(QString::fromStdString(myfen));

    // m_boardWidget->setBoardState(nowRows);
    processAITurn();

    if (m_game->isEnd(m_turnWhite))
    {
        qDebug() << "終了" << movetext;
    }
}

// FEN文字列を受け取り、合法手と最善手を生成し、boardWidgetを更新する
void MainWindow::on_fenInput_textChanged(const QString &text)
{
    std::string newBoard[8];
    QStringList ranks = text.split('/');

    // FEN形式ではないと判断した場合は処理を中断
    if (ranks.size() != 8)
        return;

    for (int i = 0; i < 8; ++i)
    {
        QString rank = ranks[i];
        std::string row_str = "";

        for (QChar pieceChar : rank)
        {
            if (pieceChar.isDigit())
            {
                int emptyCount = pieceChar.digitValue();
                for (int k = 0; k < emptyCount; ++k)
                {
                    row_str += '*';
                }
            }
            else
            {
                row_str += pieceChar.toLatin1();
            }
        }
        newBoard[i] = row_str;
    }

    // 1. GUI (描画ウィジェット)を更新
    m_boardWidget->setBoardState(newBoard);

    // 2. ChessGameに状態を設定
    m_game->initBoardWithStrings(newBoard);
    // bool turnWhite = m_game->turn; // 仮に白番として処理 (FENから読み取るべき情報だが、元のコードに合わせて暫定的に固定)

    // 3. 合法手と最善手を生成
    std::vector<Move> legalMoves = m_game->generateMoves(m_turnWhite);
    Move best = m_game->bestMove(m_turnWhite);

    // 4. GUI表示用の文字列に変換
    QStringList moveStrings;
    for (const auto &move : legalMoves)
    {
        std::string start_alg = m_game->coordsToAlgebraic(move.first.first, move.first.second);
        std::string end_alg = m_game->coordsToAlgebraic(move.second.first, move.second.second);
        moveStrings.append(QString::fromStdString(start_alg + end_alg));
    }

    QString bestString;
    std::string start_alg = m_game->coordsToAlgebraic(best.first.first, best.first.second);
    std::string end_alg = m_game->coordsToAlgebraic(best.second.first, best.second.second);
    bestString = QString::fromStdString(start_alg + end_alg);

    std::string mycommand = command(newBoard, best);

    QString commandString = QString::fromStdString(mycommand);

    // 5. ラベルを更新
    QString prefix;

    prefix = QString("Legal Moves (") + (m_turnWhite ? "White" : "Black") + "): ";
    m_moveListLabel->setText(prefix + moveStrings.join(", "));

    prefix = QString("Best Move (") + (m_turnWhite ? "White" : "Black") + "): ";
    m_bestMoveLabel->setText(prefix + bestString);

    prefix = QString("Command for Arduino: \n");
    m_commandLabel->setText(prefix + commandString);
}

void MainWindow::handleSerialError(const QString &message)
{
    QMessageBox::critical(this, "Serial Error", message);
}

void MainWindow::handleMotorStatusChanged(char axis, bool isOn)
{
    // モーター状態が変化したとき、ボタンのテキストを自動更新
    if (isOn)
    {
        switch (axis)
        {
        case 'x':
            m_xmoter_button_p->setText("モーター OFF");
            m_xmoter_button_n->setText("モーター OFF");
            break;
        case 'y':
            m_ymoter_button_p->setText("モーター OFF");
            m_ymoter_button_n->setText("モーター OFF");
            break;
        case 'z':
            m_zmoter_button_p->setText("モーター OFF");
            m_zmoter_button_n->setText("モーター OFF");
            break;
        }
    }
    else
    {
        switch (axis)
        {
        case 'x':
            m_xmoter_button_p->setText("X軸 ＋");
            m_xmoter_button_n->setText("X軸 ー");
            break;
        case 'y':
            m_ymoter_button_p->setText("Y軸 ＋");
            m_ymoter_button_n->setText("Y軸 ー");
            break;
        case 'z':
            m_zmoter_button_p->setText("Z軸 ＋");
            m_zmoter_button_n->setText("Z軸 ー");
            break;
        case 'a':
            m_xmoter_button_p->setText("X軸 ＋");
            m_xmoter_button_n->setText("X軸 ー");
            m_ymoter_button_p->setText("Y軸 ＋");
            m_ymoter_button_n->setText("Y軸 ー");
            m_zmoter_button_p->setText("Z軸 ＋");
            m_zmoter_button_n->setText("Z軸 ー");
            break;
        }
    }
}

void MainWindow::processAITurn()
{
    // AIの番ではないか、FENが未設定なら何もしない
    if (m_turnWhite || m_fenInput->text().isEmpty())
        return;

    // 現在の盤面を m_game に確実に設定し直す（FEN解析ロジックを再利用）
    const QString &text = m_fenInput->text();
    std::string newBoard[8];
    QStringList ranks = text.split('/');
    if (ranks.size() != 8)
        return;

    for (int i = 0; i < 8; ++i)
    {
        QString rank = ranks[i];
        std::string row_str = "";
        for (QChar pieceChar : rank)
        {
            if (pieceChar.isDigit())
            {
                int emptyCount = pieceChar.digitValue();
                for (int k = 0; k < emptyCount; ++k)
                {
                    row_str += '*';
                }
            }
            else
            {
                row_str += pieceChar.toLatin1();
            }
        }
        newBoard[i] = row_str;
    }
    m_game->initBoardWithStrings(newBoard);

    // 1. AIの手を打つ
    Move best = m_game->bestMove(m_turnWhite);
    m_game->makeMove(best);
    m_turnWhite = true; // ターンを白に戻す

    // 2. シリアルコマンド生成（AIの手を打つ前の盤面 newBoard と最善手 best を使用）
    std::string mycommand = command(newBoard, best);

    // 3. 新しい盤面状態をFENに変換 (AIが動かした後)
    std::string nowRows[8];
    m_game->getBoardAsStrings(nowRows);
    std::string myfen = "";
    for (int i = 0; i < 8; ++i)
    {
        if (i > 0)
        {
            myfen += "/";
        }
        myfen += nowRows[i];
    }

    // 4. GUIを更新: FEN更新 -> on_fenInput_textChanged が呼ばれ、ラベルとボードが自動更新される
    m_fenInput->setText(QString::fromStdString(myfen));

    // 5. シリアルコマンドをワーカースレッドに委譲
    emit sendAiCommand(QString::fromStdString(mycommand));
}
