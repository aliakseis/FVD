#include <QObject>
#include <QtTest>
#include <QString>
#include <QList>
#include "scriptstrategy.h"

#include <QDebug>
#include <QApplication>

namespace detail
{
bool waitForSignal(QObject* sender, const char* signal, const char* signal2, int timeout = 1000);
}

class helper : public QObject
{
	Q_OBJECT
public:
	QList<SearchResult> result;
public slots:
	void onSearchResultsFound(const QList<SearchResult>&);
	void onSearchFailed(const QString&);
};

class extractHelper : public QObject
{
	Q_OBJECT
public:
	bool hasLinks;
	extractHelper() : hasLinks(false) {}

public slots:
	void onlinksExtracted(const QMap<QString, QVariant>& links, int preferredResolutionId);
signals:
	void extracted();
};

class Test_ScriptStrategies : public QObject
{
	Q_OBJECT

private Q_SLOTS:
	void initTestCase();
	void cleanupTestCase();
	void IsSearch();
	void IsExtract();

private:
	QList<QString> urlsToExtract;
	ScriptStrategy* strategy;
};


namespace detail
{
bool waitForSignal(QObject* sender, const char* signal, int timeout)
{
	QEventLoop loop;
	QTimer timer;
	timer.setInterval(timeout);
	timer.setSingleShot(true);

	loop.connect(sender, signal, SLOT(quit()));
	loop.connect(&timer, SIGNAL(timeout()), SLOT(quit()));
	timer.start();
	loop.exec();

	return timer.isActive();
}
}

void helper::onSearchResultsFound(const QList<SearchResult>& res)
{
	result = res;
}

void helper::onSearchFailed(const QString& error)
{
	QVERIFY(false);
	qWarning() << STRATEGY_NAME << "onSearchFailed:" << error;
}

void extractHelper::onlinksExtracted(const QMap<QString, QVariant>& links, int preferredResolutionId)
{
	hasLinks = links.count() > 0;
	emit extracted();
}

/********************** Tests strategies ****************************/
void Test_ScriptStrategies::initTestCase()
{
	strategy = new ScriptStrategy(STRATEGY_NAME, true);
}

void Test_ScriptStrategies::cleanupTestCase()
{
	delete strategy;
}

void Test_ScriptStrategies::IsSearch()
{
	helper h;
	VERIFY(QObject::connect(strategy, SIGNAL(searchResultsFound(QList<SearchResult>)), &h, SLOT(onSearchResultsFound(QList<SearchResult>))));
	QString searchQuery = "girl";
	strategy->search(searchQuery, strategies::kSortDateAdded, 10, 1);
	bool is_ready_request = ::detail::waitForSignal(strategy, SIGNAL(searchResultsFound(QList<SearchResult>)), 30000);
	qDebug() << "Search" << searchQuery << "on" << STRATEGY_NAME << "| result size:"  << h.result.size() << "| is_ready_request:" << is_ready_request;
	QVERIFY2(h.result.size() > 0, "Nothing found");
	std::transform(h.result.begin(), h.result.end(), std::back_inserter(urlsToExtract), [](const SearchResult & r) { return r.originalUrl; });
}

void Test_ScriptStrategies::IsExtract()
{
	if (urlsToExtract.size() == 0)
	{
		IsSearch();
	}
	QVERIFY(urlsToExtract.size() > 0);
	extractHelper hp;
	strategy->extractDirectLinks(urlsToExtract[0], &hp);
	bool is_ready_request = ::detail::waitForSignal(&hp, SIGNAL(extracted()), 30000);
	QVERIFY(is_ready_request);
	QVERIFY(hp.hasLinks);
}


#include MOC_FILE
QTEST_MAIN(Test_ScriptStrategies)

