#include "chess_board_widget.hpp"
#include <QPainter>
#include <QColor>
#include <QFont>
#include <algorithm>
#include <QDebug> // デバッグ/Qt標準マクロ利用のため（必須ではありませんが慣例）
#include <QList>
#include <QMouseEvent>

// コンストラクタ
ChessBoardWidget::ChessBoardWidget(QWidget *parent) : QWidget(parent)
{
    // 背景を黒でクリア
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
}

// 盤面状態の設定
void ChessBoardWidget::setBoardState(const std::string rows[8])
{
    m_boardRows.clear();
    for (int i = 0; i < 8; ++i)
    {
        // std::stringをQStringに変換してリストに追加
        m_boardRows.append(QString::fromStdString(rows[i]));
    }
    update(); // QWidget::update()で再描画イベントをキューに追加
}

/**
 * @brief FEN文字列を受け取り、盤面状態を更新し、再描画を要求します。
 * @param fen FEN文字列全体。駒配置フィールドのみを解析します。
 */
void ChessBoardWidget::setBoardFromFEN(const std::string &fen)
{
    // 1. 駒配置フィールドの抽出（最初のスペースまで）
    size_t firstSpace = fen.find(' ');
    std::string placement = (firstSpace == std::string::npos) ? fen : fen.substr(0, firstSpace);

    std::string rows[8];
    int rank = 0; // 0 (8ランク) から 7 (1ランク)
    std::string currentRankStr;

    for (char c : placement)
    {
        if (rank >= 8)
            break; // 8ランク分処理したら終了

        if (c == '/')
        {
            // ランクの区切り
            if (currentRankStr.length() != 8)
            {
                qWarning() << "FEN parsing error: Rank" << rank << "does not have 8 squares.";
                // エラー処理（ここでは不完全なデータで次のランクへ）
            }
            rows[rank] = currentRankStr;
            currentRankStr.clear();
            rank++;
        }
        else if (isdigit(c))
        {
            // 空マス
            int emptyCount = c - '0';
            for (int i = 0; i < emptyCount; ++i)
            {
                currentRankStr += '*'; // 空マスは'*'で表現
            }
        }
        else if (isalpha(c))
        {
            // 駒
            currentRankStr += c;
        }
        else
        {
            // その他の文字（無視またはエラー）
            qWarning() << "FEN parsing: Unexpected character:" << c;
        }
    }

    // 最後のランクを保存
    if (!currentRankStr.empty() && rank < 8)
    {
        if (currentRankStr.length() != 8)
        {
            qWarning() << "FEN parsing error: Final rank" << rank << "does not have 8 squares.";
        }
        rows[rank] = currentRankStr;
    }

    // 2. 既存の描画ロジックに渡す
    // 注意: m_boardRows の形式は'p', 'r', '*', ... であり、
    // setBoardStateの引数もこの形式である必要があります。
    setBoardState(rows);
}

void ChessBoardWidget::mousePressEvent(QMouseEvent *event)
{
    QPoint clickPos = event->pos();

    // 保持しているすべての矩形に対してループ処理を行う
    for (int i = 0; i < m_cellRects.size(); ++i)
    {
        const QRect &rect = m_cellRects.at(i);

        if (rect.contains(clickPos))
        {
            qDebug() << "クリックイベントが検出されました。矩形インデックス:" << i;
            // インデックス i に基づいて、目的の処理を実行
            event->accept();
            return; // 処理が完了したら終了
        }
    }

    QWidget::mousePressEvent(event);
}

// カスタム描画イベント
void ChessBoardWidget::paintEvent(QPaintEvent *event)
{
    // paintEventの引数eventが未使用であるという警告を抑制
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing); // アンチエイリアスを有効に

    // --- 座標表示のための設定 ---
    const int margin = 20; // 座標表示用の余白 (ピクセル)
    int boardSize;         // 描画されるチェス盤の1辺の長さ
    int squareSize;        // 1マスのサイズ
    int xOffset;           // 描画開始位置のX座標
    int yOffset;           // 描画開始位置のY座標
    const int padding = 3;

    // ボードサイズとオフセットの決定 (正方形を維持しつつ、余白を考慮して中央揃え)
    int availableWidth = width() - 2 * margin;
    int availableHeight = height() - 2 * margin;

    if (availableWidth < availableHeight)
    {
        // 縦長の場合（幅を基準）
        boardSize = availableWidth;
        squareSize = boardSize / 8;
        xOffset = margin;
        yOffset = (height() - boardSize) / 2;
    }
    else
    {
        // 横長または正方形の場合（高さを基準）
        boardSize = availableHeight;
        squareSize = boardSize / 8;
        xOffset = (width() - boardSize) / 2;
        yOffset = margin;
    }

    // boardSizeが0になる可能性を考慮 (ただし、通常は起こらない)
    if (boardSize <= 0)
        return;

    // --- 1. マス目の描画 ---
    const QColor lightColor("#474e59"); // 明るいマス
    const QColor darkColor("#a0a7ad");  // 暗いマス

    for (int rank = 0; rank < 8; ++rank)
    {
        for (int file = 0; file < 8; ++file)
        {
            // 色の決定 (奇数行/列の合計が暗い色)
            QColor color = ((rank + file) % 2 == 0) ? lightColor : darkColor;

            painter.fillRect(
                xOffset + file * squareSize,
                yOffset + rank * squareSize,
                squareSize,
                squareSize,
                color);

            m_cellRects << QRect(
                xOffset + file * squareSize,
                yOffset + rank * squareSize,
                squareSize,
                squareSize);
        }
    }

    // --- 2. 座標の描画 (行番号と列アルファベット) ---
    QFont coordFont("Arial Unicode MS", static_cast<int>(margin * 0.6));
    coordFont.setBold(false);
    painter.setFont(coordFont);

    // 列アルファベット (a, b, c, ...) - 下側
    painter.setPen(QColor("#e1eef3"));
    for (int file = 0; file < 8; file++)
    {
        char fileChar = 'a' + file;
        QString fileString(fileChar);

        QRect bottomRect(
            xOffset + file * squareSize,
            yOffset + 8 * squareSize - margin,
            squareSize - padding,
            margin - padding);
        painter.drawText(bottomRect, Qt::AlignBottom | Qt::AlignRight, fileString);
    }

    // 行番号 (8, 7, 6, ...) - 左側
    painter.setPen(QColor("#bed1d8"));
    for (int rank = 0; rank < 8; rank++)
    {
        QString rankString = QString::number(8 - rank);

        QRect leftRect(
            xOffset + padding,
            yOffset + rank * squareSize + padding,
            margin,
            squareSize);
        painter.drawText(leftRect, Qt::AlignTop | Qt::AlignLeft, rankString);
    }

    // --- 3. 駒の描画 (テキスト/絵文字) ---
    if (m_boardRows.size() != 8)
        return;

    // フォント設定 (マスに合わせてサイズを調整)
    QFont pieceFont("Arial Unicode MS", static_cast<int>(squareSize * 0.7));
    pieceFont.setBold(true);
    painter.setFont(pieceFont);

    for (int rank = 0; rank < 8; ++rank)
    {
        QString rankString = m_boardRows[rank];
        for (int file = 0; file < rankString.length(); ++file)
        {
            QChar pieceChar = rankString.at(file);

            if (pieceChar == '*')
            {
                continue; // 空マスはスキップ
            }

            QChar pieceEmoji;

            switch (pieceChar.toLower().toLatin1())
            {
            case 'p':
                pieceEmoji = QChar(L'♟');
                break;
            case 'k':
                pieceEmoji = QChar(L'♚');
                break;
            case 'q':
                pieceEmoji = QChar(L'♛');
                break;
            case 'n':
                pieceEmoji = QChar(L'♞');
                break;
            case 'b':
                pieceEmoji = QChar(L'♝');
                break;
            case 'r':
                pieceEmoji = QChar(L'♜');
                break;
            default:
                continue;
            }

            // 描画矩形を定義
            QRect targetRect(
                xOffset + file * squareSize,
                yOffset + rank * squareSize,
                squareSize,
                squareSize);

            painter.setPen(QColor("#4018181a")); // 影

            // 影
            int offsetX = 2;
            int offsetY = 3;
            painter.drawText(
                targetRect.translated(offsetX, offsetY), // 矩形をオフセット分ずらす
                Qt::AlignCenter,
                QString(pieceEmoji));

            // 白駒 (大文字)
            if (pieceChar.isUpper())
            {
                painter.setPen(Qt::white); // 白駒
            }
            // 黒駒 (小文字)
            else
            {
                painter.setPen(Qt::black); // 黒駒
            }

            // 矩形の中央に絵文字を描画
            painter.drawText(targetRect, Qt::AlignCenter, QString(pieceEmoji));
        }
    }
}