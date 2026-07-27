#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class Config;

// User settings, persisted to <root>/settings.json and exposed to QML.
//   apiBaseUrl / apiKey / model   — OpenAI-compatible endpoint config
//   streaming                      — "流式输出" toggle (default on)
//   bypassPermissions              — skip per-tool-call approval (default off)
//   compact                        — context-compression when a conversation
//                                   nears the model's max context (default on,
//                                   per CLAUDE.md "On by default")
//   theme                          — "light" | "dark"
class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl WRITE setApiBaseUrl NOTIFY
                   apiBaseUrlChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY apiKeyChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(
        QStringList models READ models WRITE setModels NOTIFY modelsChanged)
    Q_PROPERTY(QString modelsText READ modelsText WRITE setModelsText NOTIFY
                   modelsChanged)
    Q_PROPERTY(bool streaming READ streaming WRITE setStreaming NOTIFY
                   streamingChanged)
    Q_PROPERTY(bool bypassPermissions READ bypassPermissions WRITE
                   setBypassPermissions NOTIFY bypassPermissionsChanged)
    Q_PROPERTY(bool compact READ compact WRITE setCompact NOTIFY compactChanged)
    Q_PROPERTY(bool startOnLogin READ startOnLogin WRITE setStartOnLogin NOTIFY
                   startOnLoginChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY
                   closeToTrayChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString currentThemeId READ currentThemeId WRITE
                   setCurrentThemeId NOTIFY currentThemeIdChanged)
    Q_PROPERTY(QString webSearchImplementation READ webSearchImplementation
                   WRITE setWebSearchImplementation NOTIFY
                       webSearchImplementationChanged)
    Q_PROPERTY(QString webSearchModel READ webSearchModel WRITE
                   setWebSearchModel NOTIFY webSearchModelChanged)
    Q_PROPERTY(QString webSearchExternalApiKey READ webSearchExternalApiKey
                   WRITE setWebSearchExternalApiKey NOTIFY
                       webSearchExternalApiKeyChanged)
    Q_PROPERTY(QString webSearchExternalBaseUrl READ webSearchExternalBaseUrl
                   WRITE setWebSearchExternalBaseUrl NOTIFY
                       webSearchExternalBaseUrlChanged)
    Q_PROPERTY(bool globalScheduledTasksEnabled READ globalScheduledTasksEnabled
                   WRITE setGlobalScheduledTasksEnabled NOTIFY
                       globalScheduledTasksEnabledChanged)

  public:
    explicit Settings(Config *config, QObject *parent = nullptr);

    void load(); // from settings.json (no-op if absent → defaults)
    Q_INVOKABLE void save();

    QString apiBaseUrl() const { return m_apiBaseUrl; }
    QString apiKey() const { return m_apiKey; }
    QString model() const { return m_model; }
    QStringList models() const { return m_models; }
    QString modelsText() const { return m_models.join(QLatin1Char('\n')); }
    bool streaming() const { return m_streaming; }
    bool bypassPermissions() const { return m_bypassPermissions; }
    bool compact() const { return m_compact; }
    bool startOnLogin() const { return m_startOnLogin; }
    bool closeToTray() const { return m_closeToTray; }
    QString theme() const { return m_theme; }
    QString currentThemeId() const { return m_currentThemeId; }
    QString webSearchImplementation() const { return m_webSearchImplementation; }
    QString webSearchModel() const { return m_webSearchModel; }
    QString webSearchExternalApiKey() const { return m_webSearchExternalApiKey; }
    QString webSearchExternalBaseUrl() const
    {
        return m_webSearchExternalBaseUrl;
    }
    bool globalScheduledTasksEnabled() const
    {
        return m_globalScheduledTasksEnabled;
    }

    void setApiBaseUrl(const QString &v);
    void setApiKey(const QString &v);
    void setModel(const QString &v);
    void setModels(const QStringList &v);
    void setModelsText(const QString &v);
    void setStreaming(bool v);
    void setBypassPermissions(bool v);
    void setCompact(bool v);
    void setStartOnLogin(bool v);
    void setCloseToTray(bool v);
    void setTheme(const QString &v);
    void setCurrentThemeId(const QString &v);
    void setWebSearchImplementation(const QString &v);
    void setWebSearchModel(const QString &v);
    void setWebSearchExternalApiKey(const QString &v);
    void setWebSearchExternalBaseUrl(const QString &v);
    void setGlobalScheduledTasksEnabled(bool v);

  signals:
    void apiBaseUrlChanged();
    void apiKeyChanged();
    void modelChanged();
    void modelsChanged();
    void streamingChanged();
    void bypassPermissionsChanged();
    void compactChanged();
    void startOnLoginChanged();
    void closeToTrayChanged();
    void themeChanged();
    void currentThemeIdChanged();
    void webSearchImplementationChanged();
    void webSearchModelChanged();
    void webSearchExternalApiKeyChanged();
    void webSearchExternalBaseUrlChanged();
    void globalScheduledTasksEnabledChanged();

  private:
    Config *m_config;
    QString m_apiBaseUrl{"https://api.openai.com/v1"};
    QString m_apiKey;
    QString m_model{"gpt-4o-mini"};
    QStringList m_models{QStringList{QStringLiteral("gpt-4o-mini"),
                                     QStringLiteral("gpt-4.1"),
                                     QStringLiteral("gpt-5")}};
    bool m_streaming{true};
    bool m_bypassPermissions{false};
    bool m_compact{true};
    bool m_startOnLogin{false};
    bool m_closeToTray{true};
    QString m_theme{"light"};
    QString m_currentThemeId{"warm-clay"};
    QString m_webSearchImplementation{"bing_legacy"};
    QString m_webSearchModel{"gpt-4o-mini"};
    QString m_webSearchExternalApiKey;
    QString m_webSearchExternalBaseUrl{"https://api.openai.com/v1"};
    bool m_globalScheduledTasksEnabled{true};

    void persist(); // serialize current state to settings.json
};
