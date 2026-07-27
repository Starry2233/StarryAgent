#include "ConversationManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

#include "ScheduledTaskManager.h"
#include "core/Config.h"
#include "core/DebugTrace.h"
#include "core/Settings.h"
#include "tools/ToolRegistry.h"

namespace
{
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream stream(&file);
    return stream.readAll().trimmed();
}

QString loadSkillsPrompt(const QString &skillsDir)
{
    if (skillsDir.isEmpty())
        return {};

    QStringList blocks;
    QDirIterator it(skillsDir, {QStringLiteral("SKILL.md")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString path = it.next();
        const QString raw = readTextFile(path);
        if (raw.isEmpty())
            continue;

        QString name = QFileInfo(path).dir().dirName();
        QString description;
        QString body = raw;
        if (raw.startsWith(QStringLiteral("---\n")))
        {
            const int secondFence = raw.indexOf(QStringLiteral("\n---\n"), 4);
            if (secondFence > 0)
            {
                const QString frontmatter = raw.mid(4, secondFence - 4);
                body = raw.mid(secondFence + 5).trimmed();
                const QStringList lines = frontmatter.split(QLatin1Char('\n'));
                for (const QString &line : lines)
                {
                    const int colon = line.indexOf(QLatin1Char(':'));
                    if (colon <= 0)
                        continue;
                    const QString key = line.left(colon).trimmed();
                    const QString value = line.mid(colon + 1).trimmed();
                    if (key == QStringLiteral("name") && !value.isEmpty())
                        name = value;
                    else if (key == QStringLiteral("description"))
                        description = value;
                }
            }
        }

        QString block = QStringLiteral("## %1").arg(name);
        if (!description.isEmpty())
            block += QStringLiteral("\nDescription: %1").arg(description);
        if (!body.isEmpty())
            block += QStringLiteral("\n%1").arg(body);
        blocks.append(block.trimmed());
    }

    return blocks.join(QStringLiteral("\n\n"));
}

} // namespace

ConversationManager::ConversationManager(Config *config, Settings *settings,
                                         ToolRegistry *registry,
                                         bool loadHistory, QObject *parent)
    : QObject(parent), m_config(config), m_settings(settings),
      m_registry(registry)
{
    DebugTrace::verbose("conversation-manager",
                        QStringLiteral("ctor loadHistory=%1 root=%2")
                            .arg(loadHistory)
                            .arg(m_config ? m_config->rootDir() : QString()));
    if (m_config)
    {
        m_indexMd = m_config->loadIndexMd();
        m_skillsMd = loadSkillsPrompt(m_config->skillsPath());
        QDir().mkpath(conversationsDir());
        if (loadHistory)
            loadAll();
    }
    // Ensure there's always an active conversation.
    if (!m_active && !m_list.isEmpty())
        setActive(m_list.first());
}

ConversationManager::~ConversationManager()
{
    DebugTrace::verbose(
        "conversation-manager",
        QStringLiteral("dtor active=%1 count=%2")
            .arg(m_active ? m_active->id() : QStringLiteral("<null>"))
            .arg(m_list.size()));
    if (m_active)
        save(m_active);
}

QQmlListProperty<Conversation> ConversationManager::conversations()
{
    return QQmlListProperty<Conversation>(this, &m_list);
}

QString ConversationManager::conversationsDir() const
{
    return m_config ? m_config->rootDir() + QDir::separator() + "conversations"
                    : QString();
}

QString ConversationManager::plansDir() const
{
    return m_config ? m_config->plansPath() : QString();
}

void ConversationManager::observeConversation(Conversation *c)
{
    if (!c)
        return;

    c->setAttachmentsDir(conversationsDir() + QDir::separator() +
                         "attachments" + QDir::separator() + c->id());
    c->setPlanFilePath(plansDir() + QDir::separator() + c->id() +
                       QStringLiteral(".md"));

    connect(c, &Conversation::updatedChanged, this,
            [this, c]
            {
                save(c);
                emit conversationsChanged();
            });
    connect(c, &Conversation::errorChanged, this, [this, c] { save(c); });
    if (m_scheduledTasks)
    {
        connect(c, &Conversation::scheduledTaskFinished, m_scheduledTasks,
                &ScheduledTaskManager::completeRun);
    }
}

void ConversationManager::setScheduledTaskManager(ScheduledTaskManager *manager)
{
    if (m_scheduledTasks == manager)
        return;
    m_scheduledTasks = manager;
    if (!m_scheduledTasks)
        return;
    for (Conversation *conversation : m_list)
    {
        connect(conversation, &Conversation::scheduledTaskFinished,
                m_scheduledTasks, &ScheduledTaskManager::completeRun,
                Qt::UniqueConnection);
    }
}

void ConversationManager::loadAll()
{
    const QDir d(conversationsDir());
    if (!d.exists())
        return;
    const auto files = d.entryList({QStringLiteral("*.json")}, QDir::Files);
    DebugTrace::verbose("conversation-manager",
                        QStringLiteral("loadAll dir=%1 files=%2")
                            .arg(d.absolutePath())
                            .arg(files.size()));
    for (const QString &f : files)
    {
        QFile file(d.absoluteFilePath(f));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject obj =
            QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        Conversation *c =
            Conversation::fromJson(obj, m_indexMd, m_skillsMd, this);
        if (c)
        {
            DebugTrace::verbose("conversation-manager",
                                QStringLiteral("loaded file=%1 id=%2 rows=%3")
                                    .arg(f, c->id())
                                    .arg(c->rowCount()));
            c->setContext(m_settings, m_registry);
            c->setDefaultWorkdir(m_config ? m_config->workspacePath()
                                          : QString());
            observeConversation(c);
            m_list.append(c);
        }
    }
    // Sort by updated descending (most recent first).
    std::sort(m_list.begin(), m_list.end(), [](Conversation *a, Conversation *b)
              { return a->updated() > b->updated(); });
    if (!m_list.isEmpty())
        emit conversationsChanged();
}

void ConversationManager::save(Conversation *c) const
{
    if (!c || !m_config)
        return;
    const QString path =
        conversationsDir() + QDir::separator() + c->id() + ".json";
    DebugTrace::verbose("conversation-manager",
                        QStringLiteral("save id=%1 path=%2 rows=%3")
                            .arg(c->id(), path)
                            .arg(c->rowCount()));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(c->toJson()).toJson(QJsonDocument::Indented));
}

void ConversationManager::saveActive()
{
    if (m_active)
        save(m_active);
}

Conversation *ConversationManager::newConversation(const QString &modeId)
{
    auto *c =
        new Conversation(Modes::fromId(modeId), m_indexMd, m_skillsMd, this);
    c->setContext(m_settings, m_registry);
    c->setDefaultWorkdir(m_config ? m_config->workspacePath() : QString());
    observeConversation(c);
    m_list.prepend(c);
    emit conversationsChanged();
    setActive(c);
    save(c);
    DebugTrace::verbose(
        "conversation-manager",
        QStringLiteral("newConversation id=%1 mode=%2").arg(c->id(), modeId));
    return c;
}

void ConversationManager::setActive(Conversation *c)
{
    if (m_active == c)
        return;
    if (m_active)
        save(m_active);
    m_active = c;
    DebugTrace::verbose(
        "conversation-manager",
        QStringLiteral("setActive id=%1")
            .arg(m_active ? m_active->id() : QStringLiteral("<null>")));
    emit activeChanged();
}

Conversation *ConversationManager::findById(const QString &id) const
{
    for (Conversation *conversation : m_list)
        if (conversation && conversation->id() == id)
            return conversation;
    return nullptr;
}

void ConversationManager::remove(Conversation *c)
{
    if (!c)
        return;
    const int idx = m_list.indexOf(c);
    if (idx < 0)
        return;
    // remove file
    const QString path =
        conversationsDir() + QDir::separator() + c->id() + ".json";
    QFile::remove(path);
    if (m_scheduledTasks)
        m_scheduledTasks->removeConversationTasks(c->id());
    m_list.removeAt(idx);
    if (m_active == c)
    {
        m_active = nullptr;
        if (!m_list.isEmpty())
            m_active = m_list.first();
        emit activeChanged();
    }
    c->deleteLater();
    emit conversationsChanged();
}

void ConversationManager::rename(Conversation *c, const QString &newTitle)
{
    if (!c)
        return;
    const QString t = newTitle.trimmed();
    if (t.isEmpty() || t == c->title())
        return;
    c->setTitle(t); // emits titleChanged
    save(c);
    emit conversationsChanged(); // refresh sidebar mirror
}

// (No model reset needed — QQmlListProperty exposes the list directly; QML
// Repeater re-evaluates on conversationsChanged.)
