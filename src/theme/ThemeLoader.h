#pragma once

#include <QString>

#include "ThemeMetadata.h"

class ThemeLoader
{
  public:
    static bool extractTheme(const QString &archivePath, const QString &destDir,
                             QString *error = nullptr);
    static ThemeMetadata loadTheme(const QString &themePath,
                                   QString *error = nullptr);
    static bool loadThemeFonts(const ThemeMetadata &theme,
                               QString *error = nullptr);

  private:
    static bool extractTarZst(const QString &archivePath, const QString &destDir,
                              QString *error);
};
