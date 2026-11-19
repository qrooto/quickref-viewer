#pragma once
#include <QByteArray>
#include <QString>

// One kernel path-resolution operation, anchored to the supplied directory.
extern "C" int quickrefOpenBeneath(int directory, const char *relative, int flags);

namespace SafeIo {
QString systemError();
bool publishNew(int directory, const QByteArray &name, const QByteArray &contents, QString &error);
bool installSkill(const QString &codexHome, QString &error);
}
