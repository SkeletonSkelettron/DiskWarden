#include "smartattributemodel.h"

#include "appsettings.h"
#include "formatting.h"

#include <QLocale>

SmartAttributeModel::SmartAttributeModel(QObject *parent)
    : QAbstractListModel(parent)
{
    connect(AppSettings::instance(), &AppSettings::changed, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1), {DataRole});
    });
}

void SmartAttributeModel::setRows(const QList<Row> &rows, bool nvme, const QSet<int> &disabled,
                                  const QHash<int, qint64> &offsets)
{
    bool sameShape = rows.size() == m_rows.size() && nvme == m_nvme;
    for (int i = 0; sameShape && i < rows.size(); ++i)
        sameShape = rows[i].attribute.id == m_rows[i].attribute.id;

    m_nvme = nvme;
    m_disabled = disabled;
    m_offsets = offsets;
    if (sameShape) {
        m_rows = rows;
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1));
    } else {
        beginResetModel();
        m_rows = rows;
        endResetModel();
    }
    emit changed();
}

int SmartAttributeModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QString SmartAttributeModel::dataText(const Row &row) const
{
    const hg::SmartAttribute &a = row.attribute;
    if (!a.rawKnown)
        return QStringLiteral("?");
    if (AppSettings::instance()->hexData())
        return hg::formatRawHex(a.raw);
    QString text = QLocale().toString(a.raw);
    if (!row.unit.isEmpty())
        text += QLatin1Char(' ') + row.unit;
    return text;
}

QVariant SmartAttributeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &row = m_rows[index.row()];
    const hg::SmartAttribute &a = row.attribute;
    auto number = [](int v) { return v >= 0 ? QString::number(v) : QString(); };
    switch (role) {
    case IdRole:
        return a.id;
    case Qt::DisplayRole:
    case NameRole:
        return a.name;
    case ThresholdRole:
        return number(a.threshold);
    case ValueRole:
        return number(a.value);
    case WorstRole:
        return number(a.worst);
    case StatusRole:
        return row.status;
    case LevelRole:
        return m_disabled.contains(a.id) ? 0 : row.level;
    case DataRole:
        return dataText(row);
    case OffsetRole:
        return m_offsets.value(a.id, 0);
    case EnabledRole:
        return !m_disabled.contains(a.id);
    case FlagsRole:
        return m_nvme ? QString() : hg::attributeFlagsText(a.flags);
    case NormalizedRole:
        return a.value;
    default:
        return {};
    }
}

QHash<int, QByteArray> SmartAttributeModel::roleNames() const
{
    return {
        {IdRole, "number"},
        {NameRole, "name"},
        {ThresholdRole, "threshold"},
        {ValueRole, "value"},
        {WorstRole, "worst"},
        {StatusRole, "status"},
        {LevelRole, "level"},
        {DataRole, "data"},
        {OffsetRole, "offset"},
        {EnabledRole, "enabled"},
        {FlagsRole, "flags"},
        {NormalizedRole, "normalized"},
    };
}

int SmartAttributeModel::attributeId(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row].attribute.id : -1;
}

QString SmartAttributeModel::attributeName(int row) const
{
    return row >= 0 && row < m_rows.size() ? m_rows[row].attribute.name : QString();
}

QString SmartAttributeModel::flagsText(int row) const
{
    return row >= 0 && row < m_rows.size() && !m_nvme ? hg::attributeFlagsText(m_rows[row].attribute.flags) : QString();
}
