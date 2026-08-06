#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class FilePicker : public QObject
{
    Q_OBJECT
  public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QStringList pickImages();
    Q_INVOKABLE QString pickThemePackage();
    Q_INVOKABLE QString pickSkillPackage();

  signals:
    void imagesPicked(const QStringList &paths);
    void themePackagePicked(const QString &path);
    void skillPackagePicked(const QString &path);
    void errorOccurred(const QString &message);
};
