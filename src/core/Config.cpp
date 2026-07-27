#include "Config.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <QUrl>

namespace
{
constexpr const char *kStarryAgentDir = ".starryagent";
constexpr const char *kMarkerFile = "root_path.txt";
constexpr const char *kToolsJsonc = "tools.jsonc";
constexpr const char *kIndexMd = "index.md";
constexpr const char *kSettingsJson = "settings.json";
constexpr const char *kScheduledTasksJson = "scheduled_tasks.json";
constexpr const char *kWorkspace = "workspace";
constexpr const char *kSkills = "skills";
constexpr const char *kMemories = "memories";
constexpr const char *kPlans = "plans";
constexpr const char *kThemes = "themes";

// Default tools.jsonc: built-in registry enabled. Custom mcp/cli entries are
// added by the user (schema defined in phase 4 when ToolRegistry lands).
constexpr const char *kDefaultToolsJsonc =
    "[\n"
    "    {\n"
    "        \"id\": \"__built_in\",\n"
    "        \"enabled\": true\n"
    "    }\n"
    "    // custom entries:\n"
    "    // { \"id\": \"local_echo\", \"custom\": true, \"type\": \"cli\", "
    "\"enabled\": true,\n"
    "    //   \"description\": \"Send args JSON to a local command\",\n"
    "    //   \"schema\": { \"type\": \"object\", \"properties\": { \"text\": "
    "{ \"type\": \"string\" } } },\n"
    "    //   \"config\": { \"command\": \"python\", \"args\": [\"tool.py\"], "
    "\"input_mode\": \"stdin_json\" } }\n"
    "    // { \"id\": \"filesystem\", \"custom\": true, \"type\": \"mcp\", "
    "\"enabled\": true,\n"
    "    //   \"config\": { \"command\": \"npx\", \"args\": [\"-y\", "
    "\"@modelcontextprotocol/server-filesystem\", \".\"] } }\n"
    "]\n";

constexpr const char *kDefaultIndexMd =
    "# StarryAgent\n\n"
    "Notes appended to the system prompt live here.\n";
} // namespace

Config::Config(QObject *parent) : QObject(parent)
{
    readMarker(); // populates m_rootDir if the user already chose a root
}

QString Config::appDataDir() const
{
    // AppDataLocation respects org/app name set in main.cpp →
    // %APPDATA%/StarryAgent/StarryAgent
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString Config::markerPath() const
{
    return appDataDir() + QDir::separator() + QString::fromLatin1(kMarkerFile);
}

bool Config::readMarker()
{
    QFile f(markerPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream s(&f);
    QString path = s.readAll().trimmed();
    if (path.isEmpty())
        return false;
    if (!QDir(path).exists())
        return false; // stale marker — treat as first launch again
    m_rootDir = path;
    return true;
}

void Config::writeMarker(const QString &path) const
{
    QDir().mkpath(appDataDir());
    QFile f(markerPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QTextStream s(&f);
        s << path;
    }
}

QString Config::defaultRoot() const
{
    return appDataDir() + QDir::separator() +
           QString::fromLatin1(kStarryAgentDir);
}

QStringList Config::presetRoots() const
{
    const QString home = QDir::homePath();
    return {
        defaultRoot(), // %APPDATA%/.../.starryagent
        home + QDir::separator() + QString::fromLatin1(kStarryAgentDir),
        home + "/Documents/StarryAgent" + QDir::separator() +
            QString::fromLatin1(kStarryAgentDir),
    };
}

bool Config::setRoot(const QString &path)
{
    if (path.isEmpty())
        return false;
    m_rootDir = path;
    ensureStructure();
    writeMarker(path);
    emit rootDirChanged();
    return true;
}

bool Config::resolveRoot(bool useDefault)
{
    return useDefault ? setRoot(defaultRoot()) : setRoot(m_rootDir);
}

void Config::ensureStructure()
{
    if (m_rootDir.isEmpty())
        return;
    QDir root(m_rootDir);
    root.mkpath(".");
    root.mkpath(QString::fromLatin1(kWorkspace));
    root.mkpath(QString::fromLatin1(kSkills));
    root.mkpath(QString::fromLatin1(kMemories));
    root.mkpath(QString::fromLatin1(kPlans));
    root.mkpath(QString::fromLatin1(kThemes));
    writeDefaultToolsJsonc();
    writeDefaultIndexMd();
}

void Config::writeDefaultToolsJsonc() const
{
    const QString p =
        rootDir() + QDir::separator() + QString::fromLatin1(kToolsJsonc);
    if (QFile::exists(p))
        return;
    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        f.write(kDefaultToolsJsonc);
}

void Config::writeDefaultIndexMd() const
{
    const QString p =
        rootDir() + QDir::separator() + QString::fromLatin1(kIndexMd);
    if (QFile::exists(p))
        return;
    QFile f(p);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        f.write(kDefaultIndexMd);
}

QString Config::toolsJsoncPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kToolsJsonc);
}
QString Config::indexMdPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kIndexMd);
}
QString Config::workspacePath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kWorkspace);
}
QString Config::skillsPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kSkills);
}
QString Config::memoriesPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kMemories);
}
QString Config::plansPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kPlans);
}
QString Config::themesPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kThemes);
}
QString Config::settingsPath() const
{
    return rootDir() + QDir::separator() + QString::fromLatin1(kSettingsJson);
}
QString Config::scheduledTasksPath() const
{
    return rootDir() + QDir::separator() +
           QString::fromLatin1(kScheduledTasksJson);
}

QString Config::localPath(const QUrl &url) const { return url.toLocalFile(); }

QString Config::loadIndexMd() const
{
    if (m_rootDir.isEmpty())
        return {};
    QFile f(indexMdPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QTextStream(&f).readAll();
}

QByteArray Config::loadToolsJsonc() const
{
    if (m_rootDir.isEmpty())
        return {};
    QFile f(toolsJsoncPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return f.readAll();
}

QByteArray Config::loadSettings() const
{
    if (m_rootDir.isEmpty())
        return {};
    QFile f(settingsPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return f.readAll();
}

bool Config::saveSettings(const QByteArray &json) const
{
    if (m_rootDir.isEmpty())
        return false;
    QFile f(settingsPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    return f.write(json) == json.size();
}
