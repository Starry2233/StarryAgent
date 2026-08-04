#pragma once

#include <QString>

struct SkillPackageMetadata
{
    QString id;
    QString name;
    QString description;
    QString installPath;
    QString skillFilePath;

    bool valid() const { return !id.isEmpty() && !installPath.isEmpty(); }
};

class SkillPackageLoader
{
  public:
    static bool installSkillPackage(const QString &archivePath,
                                    const QString &skillsRoot,
                                    SkillPackageMetadata *metadata = nullptr,
                                    QString *error = nullptr);

  private:
    static bool extractArchive(const QString &archivePath, const QString &destDir,
                               QString *error);
};