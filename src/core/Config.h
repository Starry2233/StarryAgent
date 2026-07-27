#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Resolves and owns the `.starryagent` runtime directory.
//
// Layout (per PLAN.md / CLAUDE.md):
//   <root>/index.md        — appended to the base system prompt
//   <root>/tools.jsonc     — tool registry config (__built_in + custom mcp/cli)
//   <root>/workspace/      — AI's default working directory
//   <root>/skills/         — skill packs (OpenClaw convention)
//   <root>/memories/       — memory system
//   <root>/plans/          — per-conversation plan files
//   <root>/themes/         — installed runtime themes
//   <root>/settings.json   — user settings (api config, streaming, bypass,
//   theme)
//
// The root itself is located via a marker file in AppData (so the choice
// persists across launches even though the root may live anywhere). On first
// launch the marker is absent and the UI shows DirPromptView to pick a root.
class Config : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString rootDir READ rootDir NOTIFY rootDirChanged)
    Q_PROPERTY(bool firstLaunch READ firstLaunch NOTIFY rootDirChanged)

  public:
    explicit Config(QObject *parent = nullptr);

    QString rootDir() const { return m_rootDir; }
    bool firstLaunch() const { return m_rootDir.isEmpty(); }

    // Preset roots offered on first launch (Windows-adapted; PLAN's three
    // /sdcard choices are Android-only).
    Q_INVOKABLE QString defaultRoot() const;
    Q_INVOKABLE QStringList presetRoots() const;

    // Pick a root (user-chosen or default). Creates the tree + default files,
    // writes the marker, emits rootDirChanged. Returns false on failure.
    Q_INVOKABLE bool setRoot(const QString &path);
    Q_INVOKABLE bool resolveRoot(bool useDefault);

    // File paths under the root.
    Q_INVOKABLE QString toolsJsoncPath() const;
    Q_INVOKABLE QString indexMdPath() const;
    Q_INVOKABLE QString workspacePath() const;
    Q_INVOKABLE QString skillsPath() const;
    Q_INVOKABLE QString memoriesPath() const;
    Q_INVOKABLE QString plansPath() const;
    Q_INVOKABLE QString themesPath() const;
    Q_INVOKABLE QString settingsPath() const;
    Q_INVOKABLE QString scheduledTasksPath() const;

    // Convert a QML FileDialog URL (file:///...) to a local OS path so that
    // setRoot can be called directly from QML folder-picker results.
    Q_INVOKABLE QString localPath(const QUrl &url) const;

    // Loaders (return empty if root unset / file absent).
    QString loadIndexMd() const;       // -> appended to system prompt
    QByteArray loadToolsJsonc() const; // -> parsed by ToolRegistry (phase 4)
    QByteArray loadSettings() const;   // -> parsed by Settings
    bool saveSettings(const QByteArray &json) const;

  signals:
    void rootDirChanged();

  private:
    QString m_rootDir;

    QString appDataDir() const;
    QString markerPath() const; // remembers the chosen root across launches
    bool readMarker();
    void writeMarker(const QString &path) const;
    void ensureStructure(); // create root + subdirs + default files
    void writeDefaultToolsJsonc() const;
    void writeDefaultIndexMd() const;
};
