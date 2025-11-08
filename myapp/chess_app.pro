QT += core gui widgets

CONFIG += c++1z
DEFINES += DEV_NAME=\\\"/dev/ttyUSB0\\\"

TARGET = chess_app
TEMPLATE = app

INCLUDEPATH += . \
               ../board_capture

CONFIG += link_pkgconfig
PKGCONFIG += opencv4

SOURCES += main.cpp \
           main_window.cpp \
           board_detector_worker.cpp \
           serial.cpp \
           serial_manager.cpp \
           chess/chess_game.cpp \
           route/route.cpp \
           route/king_route.cpp \
           route/knight_route.cpp \
           route/eliminate_route.cpp \
           widget/chess_board_widget.cpp \
           ../board_capture/board_detector_lib.cpp

HEADERS += main_window.hpp \
           board_detector_worker.hpp \
           serial.hpp \
           serial_manager.hpp \
           chess/chess_game.hpp \
           chess/types.hpp \
           route/route.hpp \
           route/king_route.hpp \
           route/knight_route.hpp \
           route/eliminate_route.hpp \
           widget/chess_board_widget.hpp