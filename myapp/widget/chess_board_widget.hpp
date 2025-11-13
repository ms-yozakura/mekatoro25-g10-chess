#ifndef CHESSBOARDWIDGET_H
#define CHESSBOARDWIDGET_H

#include <QWidget>
#include <QStringList>
#include <string>

class ChessBoardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChessBoardWidget(QWidget *parent = nullptr);
    /**
     * @brief 盤面状態を更新し再描画。
     * @param rows 8行のstd::string配列。各文字は駒を表し、'*'は空マス
     */
    void setBoardState(const std::string rows[8]);

    /**
     * @brief FEN文字列を受け取り、盤面状態を更新し再描画
     * @param fen FEN文字列全体。駒配置フィールドのみを解析
     */
    void setBoardFromFEN(const std::string &fen);

    // ウィジェットの推奨サイズを設定
    QSize sizeHint() const override
    {
        return QSize(600, 600);
    }

protected:
    // カスタム描画イベント
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 盤面データをQStringListとして保持
    QStringList m_boardRows;
    QList<QRect> m_cellRects; // マスの矩形を保持するリスト
};

#endif // CHESSBOARDWIDGET_H