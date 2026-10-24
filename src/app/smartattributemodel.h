#pragma once

#include "smartattribute.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QtQml/qqmlregistration.h>

// The rows of the S.M.A.R.T. page: ATA attributes, or the fields of the
// NVMe health log presented the same way.
class SmartAttributeModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Disk")

    Q_PROPERTY(bool nvme READ isNvme NOTIFY changed)
    Q_PROPERTY(int count READ rowCount NOTIFY changed)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ThresholdRole,
        ValueRole,
        WorstRole,
        StatusRole,
        LevelRole,
        DataRole,
        OffsetRole,
        EnabledRole,
        FlagsRole,
        NormalizedRole,
    };

    struct Row {
        hg::SmartAttribute attribute;
        int level = 0;          // 0 ok, 1 warning, 2 failing
        QString status;
        QString unit;           // NVMe rows: unit of the data
    };

    explicit SmartAttributeModel(QObject *parent = nullptr);

    void setRows(const QList<Row> &rows, bool nvme, const QSet<int> &disabled,
                 const QHash<int, qint64> &offsets);

    bool isNvme() const { return m_nvme; }
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int attributeId(int row) const;
    Q_INVOKABLE QString attributeName(int row) const;
    Q_INVOKABLE QString flagsText(int row) const;

signals:
    void changed();

private:
    QString dataText(const Row &row) const;

    QList<Row> m_rows;
    bool m_nvme = false;
    QSet<int> m_disabled;
    QHash<int, qint64> m_offsets;
};
