/*
 * SPDX-FileCopyrightText: 2024 Souris
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

/*
 * Generic reusable code for SMOD
 */

#include <QDir>
#include <QFileInfo>
#include <QResource>
#include <QString>

namespace SMOD
{
    const QString SMOD_EXTENSION = QStringLiteral(".smod.rcc");
    const QString RESOURCE_PATH = QStringLiteral("smod/");

    inline void registerResource(const QString &name)
    {
        QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, RESOURCE_PATH + name + SMOD_EXTENSION);
        QResource::registerResource(path);
    }

    inline bool resourceExists(const QString &name)
    {
        QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, RESOURCE_PATH + name + SMOD_EXTENSION);
        return !path.isEmpty();
    }
}
