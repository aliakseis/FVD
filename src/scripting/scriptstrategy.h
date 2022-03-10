#pragma once

#include <QObject>
#include <QString>
#include <QFuture>

#include "scriptprovider.h"
#include "remotevideoentity.h"
#include "strategiescommon.h"

class ScriptStrategy : public QObject
{
	Q_OBJECT
public:
	ScriptStrategy(const QString& name, bool isSafe, QObject* parent = 0);
	~ScriptStrategy();

	void search(const QString& query, strategies::SortOrder order, int searchLimit, int page);
	void extractDirectLinks(const QString& videoUrl, QObject* resultReceiver);
	QString name() const {return m_strategyName;}
	bool isSafeForWork() const { return m_isSafe; }

signals:
	void searchFinished();
	void searchResultsFound(const QList<SearchResult>& results);

public slots:
	void onSearchFinished(const QVariantList& list);

private:
	Q_INVOKABLE void searchAsync(const QString& query, int order, int searchLimit, int page);
	Q_INVOKABLE void extractDirectLinksAsync(const QString& videoUrl, QObject* resultReceiver);

private:
	ScriptProvider m_scriptProvider;
	QString m_strategyName;

    QFuture<void> m_currentRunner;

	QString m_syncResult;
	bool m_isSafe;
};
