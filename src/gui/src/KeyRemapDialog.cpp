/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 */

#include "KeyRemapDialog.h"

#include <algorithm>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
constexpr int kScreenColumn = 0;
constexpr int kSourceColumn = 1;
constexpr int kOutputColumn = 2;
constexpr int kHoldOutputColumn = 3;
}

KeyRemapDialog::KeyRemapDialog(QWidget* parent, const QList<ServerConfig::KeyRemap>& remaps,
                               const QStringList& targetScreens) :
    QDialog(parent),
    m_Table(new QTableWidget(this)),
    m_TargetScreens(targetScreens)
{
    setWindowTitle(tr("Key mappings"));
    resize(760, 360);

    m_Table->setColumnCount(4);
    m_Table->setHorizontalHeaderLabels({
        tr("Target screen"), tr("Input"), tr("Output"), tr("Hold output")
    });
    m_Table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_Table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_Table->horizontalHeader()->setStretchLastSection(true);
    m_Table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    for (const auto& remap : remaps) {
        addRow(remap);
    }

    auto* addButton = new QPushButton(tr("Add"), this);
    auto* removeButton = new QPushButton(tr("Remove"), this);
    auto* presetButton = new QPushButton(tr("Add Windows to macOS preset"), this);
    connect(addButton, &QPushButton::clicked, this, &KeyRemapDialog::addRemap);
    connect(removeButton, &QPushButton::clicked, this, &KeyRemapDialog::removeSelectedRemaps);
    connect(presetButton, &QPushButton::clicked, this, &KeyRemapDialog::addMacPreset);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(addButton);
    buttons->addWidget(removeButton);
    buttons->addWidget(presetButton);
    buttons->addStretch();

    auto* dialogButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(dialogButtons, &QDialogButtonBox::accepted, this, &KeyRemapDialog::accept);
    connect(dialogButtons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_Table);
    layout->addLayout(buttons);
    layout->addWidget(dialogButtons);
}

QList<ServerConfig::KeyRemap> KeyRemapDialog::remaps() const
{
    QList<ServerConfig::KeyRemap> result;
    for (int row = 0; row < m_Table->rowCount(); ++row) {
        ServerConfig::KeyRemap remap;
        remap.screen = m_Table->item(row, kScreenColumn)->text().trimmed();
        remap.source = m_Table->item(row, kSourceColumn)->text().trimmed();
        remap.output = m_Table->item(row, kOutputColumn)->text().trimmed();
        remap.holdOutput = m_Table->item(row, kHoldOutputColumn)->text().trimmed();
        result.append(remap);
    }
    return result;
}

void KeyRemapDialog::accept()
{
    if (!validateRemaps(remaps())) {
        return;
    }
    QDialog::accept();
}

void KeyRemapDialog::addRemap()
{
    addRow();
}

void KeyRemapDialog::removeSelectedRemaps()
{
    QSet<int> rows;
    for (const auto& index : m_Table->selectionModel()->selectedRows()) {
        rows.insert(index.row());
    }
    QList<int> sortedRows = rows.values();
    std::sort(sortedRows.rbegin(), sortedRows.rend());
    for (int row : sortedRows) {
        m_Table->removeRow(row);
    }
}

void KeyRemapDialog::addMacPreset()
{
    if (m_TargetScreens.isEmpty()) {
        QMessageBox::warning(this, tr("No target screen"),
                             tr("Add the macOS client to Screens and links before creating key mappings."));
        return;
    }

    bool accepted = false;
    const QString screen = QInputDialog::getItem(this, tr("macOS target screen"),
                                                  tr("Screen name:"), m_TargetScreens, 0,
                                                  false, &accepted);
    if (!accepted) {
        return;
    }

    addRow({screen, QStringLiteral("right_alt"), QStringLiteral("next_group"), QStringLiteral("right_super")});
    addRow({screen, QStringLiteral("hangul"), QStringLiteral("next_group"), QStringLiteral("right_super")});
    addRow({screen, QStringLiteral("control+c"), QStringLiteral("command+c"), QString()});
    addRow({screen, QStringLiteral("control+v"), QStringLiteral("command+v"), QString()});
    addRow({screen, QStringLiteral("print_screen"), QStringLiteral("command+shift+4"), QString()});
}

void KeyRemapDialog::addRow(const ServerConfig::KeyRemap& remap)
{
    const int row = m_Table->rowCount();
    m_Table->insertRow(row);
    const QStringList values = {remap.screen, remap.source, remap.output, remap.holdOutput};
    for (int column = 0; column < values.size(); ++column) {
        m_Table->setItem(row, column, new QTableWidgetItem(values.at(column)));
    }
}

bool KeyRemapDialog::validateRemaps(const QList<ServerConfig::KeyRemap>& mappings) const
{
    const QRegularExpression keyPattern(QStringLiteral("^[A-Za-z0-9_+\\-]+$"));
    const QRegularExpression screenPattern(QStringLiteral("^[A-Za-z0-9_.\\-]+$"));
    QSet<QString> sources;

    for (const auto& remap : mappings) {
        if (!screenPattern.match(remap.screen).hasMatch() || !m_TargetScreens.contains(remap.screen) ||
            remap.source.isEmpty() || remap.output.isEmpty() ||
            !keyPattern.match(remap.source).hasMatch() ||
            !keyPattern.match(remap.output).hasMatch() ||
            (!remap.holdOutput.isEmpty() &&
             (!keyPattern.match(remap.holdOutput).hasMatch() || remap.holdOutput.contains('+')))) {
            QMessageBox::warning(const_cast<KeyRemapDialog*>(this), tr("Invalid key mapping"),
                                 tr("Every mapping needs a configured target screen, input, and output. Key names may contain letters, numbers, underscores, hyphens, and plus signs, except for hold output."));
            return false;
        }

        const QString identity = remap.screen + QChar('\0') + remap.source;
        if (sources.contains(identity)) {
            QMessageBox::warning(const_cast<KeyRemapDialog*>(this), tr("Duplicate key mapping"),
                                 tr("Each input key can be mapped only once for a target screen."));
            return false;
        }
        sources.insert(identity);
    }
    return true;
}
