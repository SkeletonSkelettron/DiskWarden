#pragma once

#include "historydb.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// Log or alert entries of a disk, newest first.
class LogModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Disk")

    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role { TimeRole = Qt::UserRole + 1, TextRole, LevelRole };

    explicit LogModel(QObject *parent = nullptr);

    void setEntries(const QList<HistoryDb::Event> &entries);
    void prepend(const HistoryDb::Event &entry);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private:
    QList<HistoryDb::Event> m_entries;
};
