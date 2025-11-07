#pragma once

#include <vector>
#include <string>

#include "types.hpp"

#define CELLSIZE 25.0 // マスのサイズは25mm

std::string kingRoute(std::string rows[8], Move move);