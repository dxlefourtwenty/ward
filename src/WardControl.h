#pragma once

#include <QObject>

class WardControl : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.dxle.ward.Control")

public:
    explicit WardControl(QObject *parent = nullptr);

signals:
    void reloadRequested();

public slots:
    void Reload();
};
