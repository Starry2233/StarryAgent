#include "LanguageManager.h"

#include "Settings.h"

#include <QQmlEngine>
#include <QCoreApplication>
#include <QTranslator>
#include <QVariantMap>

namespace
{
QVariantMap makeLanguageOption(const QString &value, const QString &label)
{
    QVariantMap option;
    option.insert(QStringLiteral("value"), value);
    option.insert(QStringLiteral("label"), label);
    return option;
}
}

LanguageManager::LanguageManager(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings), m_translator(new QTranslator(this))
{
}

LanguageManager::~LanguageManager() = default;

QString LanguageManager::currentLanguage() const
{
    return m_settings ? m_settings->language() : QStringLiteral("zh_CN");
}

QVariantList LanguageManager::availableLanguages() const
{
    return {
        makeLanguageOption(QStringLiteral("zh_CN"), QStringLiteral("简体中文")),
        makeLanguageOption(QStringLiteral("zh_TW"), QStringLiteral("繁體中文")),
        makeLanguageOption(QStringLiteral("en_US"), QStringLiteral("English (US)")),
    };
}

void LanguageManager::initialize()
{
    applyLanguage(currentLanguage());
}

void LanguageManager::bindEngine(QQmlEngine *engine)
{
    m_engine = engine;
}

void LanguageManager::setCurrentLanguage(const QString &language)
{
    if (!m_settings)
        return;
    const QString before = m_settings->language();
    m_settings->setLanguage(language);
    const QString after = m_settings->language();
    if (before == after)
        return;
    applyLanguage(after);
    emit currentLanguageChanged();
}

bool LanguageManager::applyLanguage(const QString &language)
{
    QCoreApplication::removeTranslator(m_translator);

    const QString path = translationResourcePath(language);
    const bool loaded = m_translator->load(path);
    if (loaded)
        QCoreApplication::installTranslator(m_translator);
    if (m_engine)
        m_engine->retranslate();
    return loaded;
}

QString LanguageManager::translationResourcePath(const QString &language) const
{
    return QStringLiteral(":/i18n/translations/starryagent_%1.qm").arg(language);
}
