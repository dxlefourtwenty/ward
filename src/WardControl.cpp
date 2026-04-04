#include "WardControl.h"

WardControl::WardControl(QObject *parent)
    : QObject(parent) {}

void WardControl::Reload()
{
    emit reloadRequested();
}
