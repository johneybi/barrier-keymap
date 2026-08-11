/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#pragma once

#include <QDialog>
#include <QStringList>

#include "ServerConfig.h"

class QTableWidget;

class KeyRemapDialog : public QDialog
{
    Q_OBJECT

public:
    KeyRemapDialog(QWidget* parent, const QList<ServerConfig::KeyRemap>& remaps,
                   const QStringList& targetScreens);
    QList<ServerConfig::KeyRemap> remaps() const;

public slots:
    void accept() override;

private slots:
    void addRemap();
    void removeSelectedRemaps();
    void addMacPreset();

private:
    void addRow(const ServerConfig::KeyRemap& remap = {});
    bool validateRemaps(const QList<ServerConfig::KeyRemap>& remaps) const;

    QTableWidget* m_Table;
    QStringList m_TargetScreens;
};
