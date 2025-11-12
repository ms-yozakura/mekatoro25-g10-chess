
#include "knight_route.hpp"

RotationInfo getRotationInfo(int dr, int dc)
{
    RotationInfo info;
    info.dr_sign = (dr > 0) ? 1 : -1;
    info.dc_sign = (dc > 0) ? 1 : -1;
    // ナイトの移動は (1, 2) または (2, 1) の絶対値を持つ
    // abs(dr)が2の場合、swap_axesはfalse (drが主軸)
    info.swap_axes = (std::abs(dr) == 1);
    return info;
}

bool isObstacle(const std::string rows[8], int start_r, int start_c, const RotationInfo &rot, int rel_r_norm, int rel_c_norm)
{
    int check_r, check_c;

    // 座標の逆変換 (論理は変更なし)
    if (!rot.swap_axes)
    {
        check_r = start_r + rot.dr_sign * rel_r_norm;
        check_c = start_c + rot.dc_sign * rel_c_norm;
    }
    else
    {
        check_r = start_r + rot.dr_sign * rel_c_norm;
        check_c = start_c + rot.dc_sign * rel_r_norm;
    }

    // 盤面の範囲チェック
    if (check_r < 0 || check_r > 7 || check_c < 0 || check_c > 7)
        return false;

    // 修正点: アルファベット（駒）がある場合を障害物とする
    char cell_content = rows[check_r][check_c];

    // '*' は障害物ではないため無視
    if (cell_content == ' ')
        return false; // 空白マスも障害物ではない

    // cell_content がアルファベット（駒）であれば true
    return std::isalpha(static_cast<unsigned char>(cell_content));
}

void generateCurve(std::string &text, double r, double c, double nr, double nc, const RotationInfo &rot, bool is_outer_curve)
{
    // 進行方向の距離 (常に2) と垂直方向の距離 (常に1)
    double d_major = (double)CELLSIZE * 2; // 2マス分の距離
    double d_minor = (double)CELLSIZE * 1; // 1マス分の距離

    // ナイトの移動距離の二乗: (2*L)^2 + (1*L)^2 = 5*L^2
    double dist_sq = d_major * d_major + d_minor * d_minor;

    // 元のコードで使われていた半径計算 (円の中心が始点の c 座標にあると仮定)
    // R = (dr^2 + dc^2) / (2*dc_actual)
    // dc_actual は、常に '1マス' の移動距離
    double radius = dist_sq / (2.0 * d_minor);

    // // 円の中心座標 (正規化された座標系で (r_start, c_start - R) または (r_start, c_start + R) )
    // double center_r = r;
    // double center_c = c;

    // // カーブの中心をどこに置くか: (r, c) から垂直方向に radius だけ離れた位置
    // // outer_curve (外側): 進行方向に対して左の障害物を避ける -> 右に曲がる
    // // inner_curve (内側): 進行方向に対して右の障害物を避ける -> 左に曲がる

    // // 円の中心を計算
    // if (is_outer_curve)
    // {
    //     center_c += rot.dc_sign * radius; // 外側に
    // }
    // else
    // {
    //     center_c -= rot.dc_sign * radius; // 内側に (元のコードのロジックを模倣)
    // }

    // ナイトの移動後の目標相対座標 (dr_actual, dc_actual)
    double dr_actual = (nr - r);
    double dc_actual = (nc - c);

    // 角度の計算
    // angle = arccos((R - |dc_actual|) / R)
    double angle = std::acos((std::abs(radius) - std::abs(dc_actual)) / std::abs(radius));

    int step = ARC_STEP; // アニメーションのステップ数 (元のコードでは 5)

    for (int i = 1; i <= step; i++)
    {
        double current_angle = (double)i * angle / step;
        double rr, cc;

        // **正規化された座標系で点を計算**
        double c_norm, r_norm;

        // C_norm = Center_C + R * (1 - cos(angle))
        // R_norm = Center_R + R * sin(angle)

        if (is_outer_curve)
        {
            // 外側カーブ: 角度を 0 から angle まで進める
            c_norm = radius * (1.0 - std::cos(current_angle));
            r_norm = radius * std::sin(current_angle);
        }
        else
        {
            // 内側カーブ: 角度を angle から 0 まで戻るように進める
            // (元のコードの複雑なロジックを模倣)
            c_norm = std::abs(dc_actual) - radius * (1.0 - std::cos(angle - current_angle));
            r_norm = std::abs(dr_actual) - radius * std::sin(angle - current_angle);
        }

        // **計算した正規化座標 (r_norm, c_norm) を実際の盤面座標に逆変換**
        // (r, c) を原点とする
        if (!rot.swap_axes)
        {
            // (2, 1)系
            rr = r + rot.dr_sign * r_norm;
            cc = c + rot.dc_sign * c_norm;
        }
        else
        {
            // (1, 2)系: rとcをスワップ
            rr = r + rot.dr_sign * c_norm;
            cc = c + rot.dc_sign * r_norm;
        }

        text += "MOVE(" + std::to_string(rr) + "," + std::to_string(cc) + ")\n";
    }

    text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
}

std::string knightRoute(std::string rows[8], Move move)
{
    std::string text = "";

    // 1. 座標の計算 (元のコードと同じ)
    double r = (double)CELLSIZE * move.from.first + CELLSIZE / 2;
    double c = (double)CELLSIZE * move.from.second + CELLSIZE / 2;
    double nr = (double)CELLSIZE * move.to.first + CELLSIZE / 2;
    double nc = (double)CELLSIZE * move.to.second + CELLSIZE / 2;

    text += "WARP(" + std::to_string(r) + "," + std::to_string(c) + ")\n";
    text += "UP\n";

    int dr = move.to.first - move.from.first;
    int dc = move.to.second - move.from.second;

    // 2. 回転情報の取得
    RotationInfo rot = getRotationInfo(dr, dc);

    // 3. 汎用的な障害物チェック
    // rel_r_norm=1, rel_c_norm=0 は進行方向の「内側/左側」に位置するマス
    bool obstacle_inner = isObstacle(rows, move.from.first, move.from.second, rot, 1, 0);

    // rel_r_norm=1, rel_c_norm=1 は進行方向の「外側/右側」に位置するマス
    bool obstacle_outer = isObstacle(rows, move.from.first, move.from.second, rot, 1, 1);

    // 4. 経路生成ロジックの統合
    if (!obstacle_inner && !obstacle_outer)
    {
        // 障害物なし -> 直線移動
        text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
    }
    else if (obstacle_inner && !obstacle_outer)
    {
        // 内側に障害物あり -> 外側へカーブ
        generateCurve(text, r, c, nr, nc, rot, true); // true: 外側カーブ
    }
    else if (!obstacle_inner && obstacle_outer)
    {
        // 外側に障害物あり -> 内側へカーブ
        generateCurve(text, r, c, nr, nc, rot, false); // false: 内側カーブ
    }
    else
    {
        // 左右両方に障害物あり -> L字の折れ線移動 (元のコードの安全策)
        // 中間点の座標計算を rot に基づき汎用的に行う必要があります。

        double half_r = (double)CELLSIZE / 2.0;

        // L字の中間点を計算: (r + r_move_component, c + c_move_component)
        double r_mid1, c_mid1;
        double r_mid2, c_mid2;

        if (!rot.swap_axes)
        {
            // (2, 1)系: r方向に2マス
            r_mid1 = r + rot.dr_sign * half_r;
            c_mid1 = c + rot.dc_sign * half_r;
            r_mid2 = nr - rot.dr_sign * half_r;
            c_mid2 = c_mid1;
        }
        else
        {
            // (1, 2)系: c方向に2マス
            r_mid1 = r + rot.dr_sign * half_r;
            c_mid1 = c + rot.dc_sign * half_r;
            r_mid2 = r_mid1;
            c_mid2 = nc - rot.dc_sign * half_r;
        }

        // 3点経由で移動 (元のコードの安全策に相当)
        text += "MOVE(" + std::to_string(r_mid1) + "," + std::to_string(c_mid1) + ")\n";
        text += "MOVE(" + std::to_string(r_mid2) + "," + std::to_string(c_mid2) + ")\n";
        text += "MOVE(" + std::to_string(nr) + "," + std::to_string(nc) + ")\n";
    }

    text += "DOWN\n";
    text += "AUTOCALIB\n";
    // text += "HOME";

    return text;
}
