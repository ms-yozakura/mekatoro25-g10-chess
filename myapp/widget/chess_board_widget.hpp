#ifndef CHESSBOARDWIDGET_H
#define CHESSBOARDWIDGET_H

#include <QWidget>
#include <QStringList>
#include <string>

class ChessBoardWidget : public QWidget {
    Q_OBJECT

public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);

    /**
     * @brief 盤面状態を更新し、再描画を要求します。
     * @param rows 8行のstd::string配列。各文字は駒を表し、'*'は空マスです。
     */
    void setBoardState(const std::string rows[8]);

    // ウィジェットの推奨サイズを設定
    QSize sizeHint() const override {
        return QSize(600, 600); 
    }

protected:
    // カスタム描画イベント
    void paintEvent(QPaintEvent *event) override;

private:
    // 盤面データをQStringListとして保持
    QStringList m_boardRows; 
};

#endif // CHESSBOARDWIDGET_H