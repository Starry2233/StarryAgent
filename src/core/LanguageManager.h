#pragma once

#include <QObject>
#include <QVariantList>

class QQmlEngine;
class QTranslator;
class Settings;

class LanguageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)
    Q_PROPERTY(QVariantList availableLanguages READ availableLanguages CONSTANT)

  public:
    explicit LanguageManager(Settings *settings, QObject *parent = nullptr);
    ~LanguageManager() override;

    QString currentLanguage() const;
    QVariantList availableLanguages() const;

    void initialize();
    void bindEngine(QQmlEngine *engine);

    Q_INVOKABLE void setCurrentLanguage(const QString &language);

  signals:
    void currentLanguageChanged();

  private:
    bool applyLanguage(const QString &language);
    QString translationResourcePath(const QString &language) const;

    Settings *m_settings;
    QQmlEngine *m_engine{nullptr};
    QTranslator *m_translator{nullptr};
};
