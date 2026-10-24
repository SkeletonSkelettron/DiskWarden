#include "logmodel.h"

#include <QLocale>

LogModel::LogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void LogModel::setEntries(const QList<HistoryDb::Event> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
    emit countChanged();
}

void LogModel::prepend(const HistoryDb::Event &entry)
{
    beginInsertRows({}, 0, 0);
    m_entries.prepend(entry);
    endInsertRows();
    emit countChanged();
}

int LogModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant LogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};
    const HistoryDb::Event &e = m_entries[index.row()];
    switch (role) {
    case TimeRole:
        return QLocale().toString(e.time, QLocale::ShortFormat);
    case Qt::DisplayRole:
    case TextRole:
        return e.text;
    case LevelRole:
        return e.level;
    default:
        return {};
    }
}

QHash<int, QByteArray> LogModel::roleNames() const
{
    return {{TimeRole, "time"}, {TextRole, "text"}, {LevelRole, "level"}};
}
