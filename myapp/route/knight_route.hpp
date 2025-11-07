#pragma once

#include <vector>
#include <string>

#include "../chess/types.hpp"

#define CELLSIZE 25.0 // マスのサイズは25mm

#define ARC_STEP 5

// 座標系の回転・反転情報を保持する構造体
struct RotationInfo
{
    int dr_sign;    // drの符号 (+1 or -1)
    int dc_sign;    // dcの符号 (+1 or -1)
    bool swap_axes; // true: 軸が入れ替わっている (L字移動: 1,2 or 2,1)
};

RotationInfo getRotationInfo(int dr, int dc);

bool isObstacle(const std::string rows[8], int start_r, int start_c, const RotationInfo &rot, int rel_r_norm, int rel_c_norm);

void generateCurve(std::string &text, double r, double c, double nr, double nc, const RotationInfo &rot, bool is_outer_curve);

std::string knightRoute(std::string rows[8], Move move);