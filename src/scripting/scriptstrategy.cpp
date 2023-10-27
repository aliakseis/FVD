#include "scriptstrategy.h"

#include "searchmanager.h"

#include "utilities/utils.h"

#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

namespace {

QString toIdentifier(QString str)
{ 
    str.replace('-', '_'); 
    return str; 
}

QString removeNonDigits(QString str)
{
    const auto start = str.begin();

    // Remove all non-digit characters from the input string
    str.truncate(std::remove_if(start, str.end(), [](QChar ch) { return !ch.isDigit(); }) - start);

    return str;
}

} // namespace

ScriptStrategy::ScriptStrategy(const QString& name, bool isSafe, QObject* parent)
    : QObject(parent), m_scriptProvider(name), m_strategyName(name.left(name.lastIndexOf('.'))), m_isSafe(isSafe)
{
    qRegisterMetaType<QList<SearchResult>>("QList<SearchResult>");
    setObjectName(name);
}

ScriptStrategy::~ScriptStrategy()
{
    if (!m_scriptProvider.doesWaitLoop())
    {
        m_currentRunner.waitForFinished();
    }
}

void ScriptStrategy::search(const QString& query, strategies::SortOrder order, int searchLimit, int page)
{
    if (m_scriptProvider.doesWaitLoop())
    {
        QMetaObject::invokeMethod(this, "searchAsync", Qt::QueuedConnection, Q_ARG(QString, query), Q_ARG(int, order),
                                  Q_ARG(int, searchLimit), Q_ARG(int, page));
    }
    else
    {
        m_currentRunner.waitForFinished();
        m_currentRunner = QtConcurrent::run(this, &ScriptStrategy::searchAsync, query, order, searchLimit, page);
    }
}

void ScriptStrategy::extractDirectLinks(const QString& videoUrl, QObject* resultReceiver)
{
    if (m_scriptProvider.doesWaitLoop())
    {
        QMetaObject::invokeMethod(this, "extractDirectLinksAsync", Qt::QueuedConnection, Q_ARG(QString, videoUrl),
                                  Q_ARG(QObject*, resultReceiver));
    }
    else
    {
        m_currentRunner.waitForFinished();
        m_currentRunner = QtConcurrent::run(this, &ScriptStrategy::extractDirectLinksAsync, videoUrl, resultReceiver);
    }
}

void ScriptStrategy::onSearchFinished(const QVariantList& list)
{
    QList<SearchResult> results;
    for (const auto& var : list)
    {
        QVariantMap map = var.toMap();
        SearchResult result;
        result.originalUrl = map["url"].toString();
        const auto& published = map["published"];
        result.published =
            published.type() == QVariant::DateTime
                ? published.toDateTime()
                : QDateTime::fromString(removeNonDigits(published.toString()).left(8),
                                        QStringLiteral("yyyyMMdd"));
        result.videoTitle = map["title"].toString();
        result.duration = map["duration"].toInt();
        result.description = map["description"].toString();
        result.id = map["id"].toString();
        result.thumbnailUrl = map["thumbnailUrl"].toString();
        result.viewCount = map["viewCount"].toInt();
        result.strategyName = m_strategyName;
        results.append(result);
    }
    emit searchResultsFound(results);
}

void ScriptStrategy::searchAsync(const QString& query, int order, int searchLimit, int page)
{
    m_scriptProvider.invokeFunction(
        toIdentifier(m_strategyName) + QStringLiteral("_search"),
        QVariantList() << query << order << searchLimit << page << QVariant::fromValue<QObject*>(this));

    SearchManager::Instance().onSearchFinished();
}

void ScriptStrategy::extractDirectLinksAsync(const QString& videoUrl, QObject* resultReceiver)
{
    m_scriptProvider.invokeFunction(toIdentifier(m_strategyName) + QStringLiteral("_extractDirectLinks"),
                                    QVariantList() << videoUrl << QVariant::fromValue<QObject*>(resultReceiver));
}
