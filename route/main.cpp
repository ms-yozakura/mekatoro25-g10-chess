
#include "route.hpp"
#include <string>

int main()
{

    // // knight demo
    // std::string ex_rows[8] = {
    //     "rnbqkbnr",  // 0
    //     "pppppp*p",  // 1
    //     "********",  // 2
    //     "********",  // 3
    //     "********",  // 4
    //     "********",  // 5
    //     "PPPPPPPP",  // 6
    //     "RNBQKBNR"}; // 7

    // Move ex_move = {{0, 6}, {2, 5}};

    // // castling demo
    // std::string ex_rows[8] = {
    //     "r***k**r",  // 0
    //     "pppppppp",  // 1
    //     "********",  // 2
    //     "********",  // 3
    //     "********",  // 4
    //     "********",  // 5
    //     "PPPPPPPP",  // 6
    //     "RNBQKBNR"}; // 7

    // Move ex_move = {{0, 4}, {0, 2}};

    // castling demo2
    std::string ex_rows[8] = {
        "r***k**r",  // 0
        "pppppppp",  // 1
        "********",  // 2
        "********",  // 3
        "********",  // 4
        "********",  // 5
        "PPPPPPPP",  // 6
        "RNBQKBNR"}; // 7

    Move ex_move = {{0, 4}, {0, 6}};

    std::cout << command(ex_rows, ex_move) << std::endl;

    return 0;
}
