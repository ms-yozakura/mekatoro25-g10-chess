// serial_manager.hpp
#pragma once

#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QString>
#include <string>

class SerialManager : public QObject
{
Q_OBJECT // QObject を継承

    public :
    // コンストラクタ: シリアルポートのデバイス名を引数に取る
    explicit SerialManager(const std::string &dev_name, QObject *parent = nullptr);
    ~SerialManager() override; // デストラクタでポートを確実にクローズする

    // パブリックな状態確認メソッド
    bool isPortOpen() const { return m_fd >= 0; }
    bool isMotorOn(char axis) const
    {
        switch (axis)
        {
        case 'x':
            return m_moter_isON[0];
        case 'y':
            return m_moter_isON[1];
        case 'z':
            return m_moter_isON[2];
        default:
            throw std::invalid_argument("Invalid axis character");
        }
    }

    // パブリックな操作メソッド
    bool openPort();  // ポートを開く処理
    void closePort(); // ポートを閉じる処理


signals:
    // 状態変化やエラーを MainWindow に通知するためのシグナル
    void portStatusChanged(bool isOpen);           // ポートが開いた/閉じた
    void motorStatusChanged(char axis, bool isOn); // モーターがON/OFFになった
    void serialError(const QString &message);      // シリアル通信でエラーが発生した場合

private:
    const std::string m_devName;                  // デバイス名 (例: "/dev/ttyACM0")
    int m_fd = -1;                                // シリアルポートのファイルディスクリプタ
    bool m_moter_isON[3] = {false, false, false}; // モーターの状態

public slots:
    // モーター制御コマンドの送信
    void sendOnPlus(char axis);
    void sendOnMinus(char axis);
    void sendOff(char axis);
    void sendCalib();
    void sendAutoCalib();

    // コマンド送信（マニュアル）
    // void sendLongCommand(const std::string &command); // 数行
    void sendCommand(const std::string &command);     // 1行(非推奨)

    void processLongCommandSlot(const QString &command); // Q_SLOT を使用
};

#endif // SERIALMANAGER_H