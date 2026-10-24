#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// Name/value rows grouped into sections, for the Information page.
class InfoModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Disk")

public:
    enum Role { SectionRole = Qt::UserRole + 1, NameRole, ValueRole };

    struct Row {
        QString section;
        QString name;
        QString value;
    };

    explicit InfoModel(QObject *parent = nullptr);

    void setRows(const QList<Row> &rows);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<Row> m_rows;
};
