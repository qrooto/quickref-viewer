QT += core gui qml quick quickcontrols2 network testlib
CONFIG += c++17 testcase
TARGET = quickref_tests
TEMPLATE = app
INCLUDEPATH += ../src
HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += quickref_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
RESOURCES += ../src/resources.qrc

# Insert hostile directory changes immediately before the real publication syscall.
QMAKE_LFLAGS += -Wl,--wrap=linkat

HEADERS += ../src/safeio.h
SOURCES += ../src/safeio.cpp
QMAKE_LFLAGS += -Wl,--wrap=quickrefOpenBeneath
