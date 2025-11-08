// mainwindow.hpp

#pragma once

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <vector>
#include <string>

#include "route/route.hpp"

// 依存するクラスの前方宣言
// SerialManager クラスは後で作成するものとして、ここでは仮に ChessGame のみ宣言
class ChessGame;
class ChessBoardWidget;
class SerialManager; // 新しくインクルード
// SerialManager を含む serial_manager.hpp をインクルードする代わりに、
// 便宜上、ここでは SerialManager の代わりに int 型の fd の参照を渡すという形で設計を進めます。
// (理想は SerialManager クラスを作成し、そのポインタを渡すことです)

class MainWindow : public QMainWindow
{
Q_OBJECT // Qt のメタオブジェクトシステムを有効にする
    public :
    // コンストラクタ: ChessGameとSerialManagerのポインタを受け取る
    explicit MainWindow(ChessGame *game, SerialManager *serialManager, QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    // --- モーター制御関連のスロット ---
    void on_moter_button_p_clicked(char axis);
    void on_moter_button_n_clicked(char axis);
    void on_moter_button_off_clicked();
    void on_calib_button_clicked();
    void on_autocalib_button_clicked();

    void on_moveinput_button_clicked();

    // --- FEN入力とゲームロジックのスロット ---
    void on_fenInput_textChanged(const QString &text);

    // --- SerialManager からのシグナルを受け取るスロット ---
    void handleSerialError(const QString &message);
    void handleMotorStatusChanged(char axis, bool isOn);

    // ユーザーの着手後にAIの着手を実行するための新しいスロット
    void processAITurn();

    //画面キャプチャのスロット
    void onVisionBoard(const QString& boardText);



private:
    // 依存オブジェクトへのポインタ
    ChessGame *m_game;
    SerialManager *m_serialManager; // SerialManager のポインタを保持

    // UIエレメント
    QLabel *m_label;
    QPushButton *m_xmoter_button_p;
    QPushButton *m_xmoter_button_n;
    QPushButton *m_ymoter_button_p;
    QPushButton *m_ymoter_button_n;
    QPushButton *m_zmoter_button_p;
    QPushButton *m_zmoter_button_n;
    QPushButton *m_moter_button_off;
    QPushButton *m_calib_button;
    QPushButton *m_autocalib_button;
    QLabel *m_moveListLabel;
    QLabel *m_bestMoveLabel;
    QLabel *m_commandLabel;
    QLabel *board_data;
    QLineEdit *m_fenInput;
    QLineEdit *m_moveInput;
    QPushButton *m_moveInputButton;
    ChessBoardWidget *m_boardWidget;

    // ヘルパー関数
    void setupUI();
    void setupConnections();

    // グローバル変数の代わりに、モーター状態を管理するローカル変数
    bool m_moter_isON = false;
    bool m_turnWhite = true;
    signals:
    // AIが生成したコマンドをSerialManagerへ渡すためのシグナル
    void sendAiCommand(const QString &command); 

};

#endif // MAINWINDOW_H