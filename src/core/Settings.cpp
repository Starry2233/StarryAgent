#include "Settings.h"
#include "Config.h"

#include <nlohmann/json.hpp>

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

using json = nlohmann::json;

namespace
{
QStringList normalizedModels(const QStringList &input)
{
    QStringList out;
    for (const QString &entry : input)
    {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty() && !out.contains(trimmed))
            out.append(trimmed);
    }
    if (out.isEmpty())
        out.append(QStringLiteral("gpt-4o-mini"));
    return out;
}

QString normalizedWebSearchImplementation(const QString &input)
{
    const QString mode = input.trimmed().toLower();
    if (mode == QStringLiteral("bing_legacy") ||
        mode == QStringLiteral("current_api") ||
        mode == QStringLiteral("aliyun_dashscope_internal") ||
        mode == QStringLiteral("external_api"))
        return mode;
    return QStringLiteral("bing_legacy");
}

QString normalizedLanguage(const QString &input)
{
    const QString language = input.trimmed();
    if (language == QStringLiteral("zh_CN") ||
        language == QStringLiteral("zh_TW") ||
        language == QStringLiteral("en_US"))
        return language;
    return QStringLiteral("zh_CN");
}

QStringList normalizedSkillIds(const QStringList &input)
{
    QStringList out;
    for (const QString &entry : input)
    {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty() && !out.contains(trimmed, Qt::CaseInsensitive))
            out.append(trimmed);
    }
    return out;
}
} // namespace

Settings::Settings(Config *config, QObject *parent)
    : QObject(parent), m_config(config)
{
}

void Settings::load()
{
    if (!m_config)
        return;
    const QByteArray data = m_config->loadSettings();
    if (data.isEmpty())
        return; // first run → keep defaults
    try
    {
        json j = json::parse(data);
        if (j.contains("apiBaseUrl"))
            m_apiBaseUrl =
                QString::fromStdString(j["apiBaseUrl"].get<std::string>());
        if (j.contains("apiKey"))
            m_apiKey = QString::fromStdString(j["apiKey"].get<std::string>());
        if (j.contains("model"))
            m_model = QString::fromStdString(j["model"].get<std::string>());
        if (j.contains("models") && j["models"].is_array())
        {
            QStringList list;
            for (const auto &entry : j["models"])
            {
                if (entry.is_string())
                    list.append(
                        QString::fromStdString(entry.get<std::string>()));
            }
            m_models = normalizedModels(list);
        }
        if (j.contains("streaming"))
            m_streaming = j["streaming"].get<bool>();
        if (j.contains("bypassPermissions"))
            m_bypassPermissions = j["bypassPermissions"].get<bool>();
        if (j.contains("compact"))
            m_compact = j["compact"].get<bool>();
        if (j.contains("startOnLogin"))
            m_startOnLogin = j["startOnLogin"].get<bool>();
        if (j.contains("closeToTray"))
            m_closeToTray = j["closeToTray"].get<bool>();
        if (j.contains("theme"))
            m_theme = QString::fromStdString(j["theme"].get<std::string>());
        if (j.contains("language"))
            m_language = normalizedLanguage(
                QString::fromStdString(j["language"].get<std::string>()));
        if (j.contains("currentThemeId"))
            m_currentThemeId =
                QString::fromStdString(j["currentThemeId"].get<std::string>())
                    .trimmed();
        if (j.contains("webSearchImplementation"))
        {
            m_webSearchImplementation = normalizedWebSearchImplementation(
                QString::fromStdString(
                    j["webSearchImplementation"].get<std::string>()));
        }
        if (j.contains("webSearchModel"))
        {
            m_webSearchModel =
                QString::fromStdString(j["webSearchModel"].get<std::string>())
                    .trimmed();
        }
        if (j.contains("webSearchExternalApiKey"))
        {
            m_webSearchExternalApiKey = QString::fromStdString(
                j["webSearchExternalApiKey"].get<std::string>());
        }
        if (j.contains("webSearchExternalBaseUrl"))
        {
            m_webSearchExternalBaseUrl = QString::fromStdString(
                j["webSearchExternalBaseUrl"].get<std::string>());
        }
        if (j.contains("globalScheduledTasksEnabled"))
            m_globalScheduledTasksEnabled =
                j["globalScheduledTasksEnabled"].get<bool>();
        if (j.contains("developerSettingsUnlocked"))
            m_developerSettingsUnlocked =
                j["developerSettingsUnlocked"].get<bool>();
        if (j.contains("developerSettingsEnabled"))
            m_developerSettingsEnabled =
                j["developerSettingsEnabled"].get<bool>();
        if (j.contains("developerThemeOnAndroidEnabled"))
            m_developerThemeOnAndroidEnabled =
                j["developerThemeOnAndroidEnabled"].get<bool>();
        if (j.contains("disabledSkillIds") && j["disabledSkillIds"].is_array())
        {
            QStringList list;
            for (const auto &entry : j["disabledSkillIds"])
            {
                if (entry.is_string())
                    list.append(
                        QString::fromStdString(entry.get<std::string>()));
            }
            m_disabledSkillIds = normalizedSkillIds(list);
        }
        if (!m_models.contains(m_model))
            m_models.prepend(m_model);
        m_models = normalizedModels(m_models);
        if (m_model.trimmed().isEmpty())
            m_model = m_models.value(0, QStringLiteral("gpt-4o-mini"));
    }
    catch (...)
    {
        // corrupt settings → keep defaults
    }
}

void Settings::save() { persist(); }

void Settings::persist()
{
    if (!m_config)
        return;
    json j;
    j["apiBaseUrl"] = m_apiBaseUrl.toStdString();
    j["apiKey"] = m_apiKey.toStdString();
    j["model"] = m_model.toStdString();
    j["models"] = json::array();
    for (const QString &model : m_models)
        j["models"].push_back(model.toStdString());
    j["streaming"] = m_streaming;
    j["bypassPermissions"] = m_bypassPermissions;
    j["compact"] = m_compact;
    j["startOnLogin"] = m_startOnLogin;
    j["closeToTray"] = m_closeToTray;
    j["theme"] = m_theme.toStdString();
    j["language"] = m_language.toStdString();
    j["currentThemeId"] = m_currentThemeId.toStdString();
    j["webSearchImplementation"] = m_webSearchImplementation.toStdString();
    j["webSearchModel"] = m_webSearchModel.toStdString();
    j["webSearchExternalApiKey"] = m_webSearchExternalApiKey.toStdString();
    j["webSearchExternalBaseUrl"] = m_webSearchExternalBaseUrl.toStdString();
    j["globalScheduledTasksEnabled"] = m_globalScheduledTasksEnabled;
    j["developerSettingsUnlocked"] = m_developerSettingsUnlocked;
    j["developerSettingsEnabled"] = m_developerSettingsEnabled;
    j["developerThemeOnAndroidEnabled"] = m_developerThemeOnAndroidEnabled;
    j["disabledSkillIds"] = json::array();
    for (const QString &skillId : m_disabledSkillIds)
        j["disabledSkillIds"].push_back(skillId.toStdString());
    m_config->saveSettings(QByteArray::fromStdString(j.dump(4)));
}

void Settings::setApiBaseUrl(const QString &v)
{
    if (m_apiBaseUrl != v)
    {
        m_apiBaseUrl = v;
        emit apiBaseUrlChanged();
        persist();
    }
}
void Settings::setApiKey(const QString &v)
{
    if (m_apiKey != v)
    {
        m_apiKey = v;
        emit apiKeyChanged();
        persist();
    }
}
void Settings::setModel(const QString &v)
{
    const QString model = v.trimmed();
    if (model.isEmpty())
        return;
    const QStringList merged = normalizedModels(QStringList(m_models) << model);
    const bool listDirty = merged != m_models;
    const bool currentDirty = m_model != model;
    if (!listDirty && !currentDirty)
        return;
    m_models = merged;
    m_model = model;
    if (listDirty)
        emit modelsChanged();
    if (currentDirty)
        emit modelChanged();
    persist();
}
void Settings::setModels(const QStringList &v)
{
    const QStringList models = normalizedModels(v);
    const QString nextModel =
        models.contains(m_model) ? m_model : models.first();
    const bool listChanged = models != m_models;
    const bool currentChanged = nextModel != m_model;
    if (!listChanged && !currentChanged)
        return;
    m_models = models;
    m_model = nextModel;
    if (listChanged)
        emit modelsChanged();
    if (currentChanged)
        emit modelChanged();
    persist();
}
void Settings::setModelsText(const QString &v)
{
    setModels(v.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                      Qt::SkipEmptyParts));
}
void Settings::setStreaming(bool v)
{
    if (m_streaming != v)
    {
        m_streaming = v;
        emit streamingChanged();
        persist();
    }
}
void Settings::setBypassPermissions(bool v)
{
    if (m_bypassPermissions != v)
    {
        m_bypassPermissions = v;
        emit bypassPermissionsChanged();
        persist();
    }
}
void Settings::setCompact(bool v)
{
    if (m_compact != v)
    {
        m_compact = v;
        emit compactChanged();
        persist();
    }
}
void Settings::setStartOnLogin(bool v)
{
    if (m_startOnLogin != v)
    {
        m_startOnLogin = v;
        emit startOnLoginChanged();
        persist();
    }
}
void Settings::setCloseToTray(bool v)
{
    if (m_closeToTray != v)
    {
        m_closeToTray = v;
        emit closeToTrayChanged();
        persist();
    }
}
void Settings::setTheme(const QString &v)
{
    if (m_theme != v)
    {
        m_theme = v;
        emit themeChanged();
        persist();
    }
}
void Settings::setLanguage(const QString &v)
{
    const QString language = normalizedLanguage(v);
    if (m_language != language)
    {
        m_language = language;
        emit languageChanged();
        persist();
    }
}
void Settings::setCurrentThemeId(const QString &v)
{
    const QString id = v.trimmed();
    if (id.isEmpty() || m_currentThemeId == id)
        return;
    m_currentThemeId = id;
    emit currentThemeIdChanged();
    persist();
}
void Settings::setWebSearchImplementation(const QString &v)
{
    const QString mode = normalizedWebSearchImplementation(v);
    if (m_webSearchImplementation != mode)
    {
        m_webSearchImplementation = mode;
        emit webSearchImplementationChanged();
        persist();
    }
}
void Settings::setWebSearchModel(const QString &v)
{
    const QString model = v.trimmed();
    if (model.isEmpty() || m_webSearchModel == model)
        return;
    m_webSearchModel = model;
    emit webSearchModelChanged();
    persist();
}
void Settings::setWebSearchExternalApiKey(const QString &v)
{
    if (m_webSearchExternalApiKey != v)
    {
        m_webSearchExternalApiKey = v;
        emit webSearchExternalApiKeyChanged();
        persist();
    }
}
void Settings::setWebSearchExternalBaseUrl(const QString &v)
{
    if (m_webSearchExternalBaseUrl != v)
    {
        m_webSearchExternalBaseUrl = v;
        emit webSearchExternalBaseUrlChanged();
        persist();
    }
}
void Settings::setGlobalScheduledTasksEnabled(bool v)
{
    if (m_globalScheduledTasksEnabled != v)
    {
        m_globalScheduledTasksEnabled = v;
        emit globalScheduledTasksEnabledChanged();
        persist();
    }
}
void Settings::setDeveloperSettingsUnlocked(bool v)
{
    if (m_developerSettingsUnlocked != v)
    {
        m_developerSettingsUnlocked = v;
        emit developerSettingsUnlockedChanged();
        persist();
    }
}
void Settings::setDeveloperSettingsEnabled(bool v)
{
    if (m_developerSettingsEnabled != v)
    {
        m_developerSettingsEnabled = v;
        emit developerSettingsEnabledChanged();
        persist();
    }
}
void Settings::setDeveloperThemeOnAndroidEnabled(bool v)
{
    if (m_developerThemeOnAndroidEnabled != v)
    {
        m_developerThemeOnAndroidEnabled = v;
        emit developerThemeOnAndroidEnabledChanged();
        persist();
    }
}

bool Settings::isSkillEnabled(const QString &skillId) const
{
    const QString id = skillId.trimmed();
    if (id.isEmpty())
        return true;
    for (const QString &disabledId : m_disabledSkillIds)
    {
        if (disabledId.compare(id, Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

void Settings::setSkillEnabled(const QString &skillId, bool enabled)
{
    const QString id = skillId.trimmed();
    if (id.isEmpty())
        return;

    QStringList next = m_disabledSkillIds;
    next = normalizedSkillIds(next);
    for (int i = next.size() - 1; i >= 0; --i)
    {
        if (next.at(i).compare(id, Qt::CaseInsensitive) == 0)
            next.removeAt(i);
    }
    if (!enabled)
        next.append(id);
    next = normalizedSkillIds(next);
    if (next == m_disabledSkillIds)
        return;
    m_disabledSkillIds = next;
    emit disabledSkillIdsChanged();
    persist();
}

void Settings::clearSkillState(const QString &skillId)
{
    setSkillEnabled(skillId, true);
}

void Settings::resetDeveloperSettings()
{
    const bool enabledChanged = m_developerSettingsEnabled;
    const bool themeChanged = m_developerThemeOnAndroidEnabled;
    if (!enabledChanged && !themeChanged)
        return;
    m_developerSettingsEnabled = false;
    m_developerThemeOnAndroidEnabled = false;
    if (enabledChanged)
        emit developerSettingsEnabledChanged();
    if (themeChanged)
        emit developerThemeOnAndroidEnabledChanged();
    persist();
}
