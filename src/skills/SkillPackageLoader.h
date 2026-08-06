#pragma once

#include <QList>
#include <QString>

struct SkillPackageMetadata
{
    QString id;
    QString name;
    QString description;
    QString installPath;
    QString skillFilePath;
    QString relativePath;

    bool valid() const { return !id.isEmpty() && !installPath.isEmpty(); }
};

struct SkillPackageInspection
{
    SkillPackageMetadata parent;
    QList<SkillPackageMetadata> children;
};

class SkillPackageLoader
{
  public:
    static bool installSkillPackage(const QString &archivePath,
                                    const QString &skillsRoot,
                                    SkillPackageMetadata *metadata = nullptr,
                                    QString *error = nullptr);
    static bool inspectSkillPackage(const QString &archivePath,
                                    SkillPackageInspection *inspection,
                                    QString *error = nullptr);
    static bool installFromPackage(const QString &archivePath,
                                   const QString &skillsRoot,
                                   const QString &relativeSkillPath,
                                   SkillPackageMetadata *metadata = nullptr,
                                   QString *error = nullptr);

  private:
    static bool extractArchive(const QString &archivePath, const QString &destDir,
                               QString *error);
};