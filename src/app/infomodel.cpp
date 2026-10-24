#include "infomodel.h"

InfoModel::InfoModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void InfoModel::setRows(const QList<Row> &rows)
{
    bool same = rows.size() == m_rows.size();
    for (int i = 0; same && i < rows.size(); ++i)
        same = rows[i].section == m_rows[i].section && rows[i].name == m_rows[i].name;
    if (same) {
        m_rows = rows;
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1), {ValueRole});
        return;
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

int InfoModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant InfoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    switch (role) {
    case SectionRole:
        return row.section;
    case Qt::DisplayRole:
    case NameRole:
        return row.name;
    case ValueRole:
        return row.value;
    default:
        return {};
    }
}

QHash<int, QByteArray> InfoModel::roleNames() const
{
    return {{SectionRole, "section"}, {NameRole, "name"}, {ValueRole, "value"}};
}
