#include <QObject>
#include <QtTest>
#include <QString>
#include <QList>
#include "strategies/strategyinterface.h"

using namespace strategies;

#include "strategies/youtubestrategy.h"
#include "strategies/vimeostrategy.h"
#include "strategies/dailymotionstrategy.h"

#include <algorithm>
#include <iterator>

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
	void onLinksExtracted(const QMap<int, LinkInfo>& directLinks, int preferredResolutionId);
};

class IQObject: public QObject
{
	Q_OBJECT
private Q_SLOTS:
	virtual void init() = 0;
	virtual void constructor() = 0;
	virtual void IsSearch() = 0;
	virtual void IsExtract() = 0;
	virtual void cleanup() = 0;
};

template<typename strategy_type>
class Test_Strategies: public IQObject
{
	void initTestCase();

	virtual void init()       override;
	virtual void constructor()override;
	virtual void IsSearch()   override;
	virtual void IsExtract()  override;
	virtual void cleanup()    override;

	void cleanupTestCase();

private:
	strategy_type strategy;
	QList<QString> urlsToExtract;
};


namespace detail
{
bool waitForSignal(QObject* sender, const char* signal, const char* signal2, int timeout)
{
	QEventLoop loop;
	QTimer timer;
	timer.setInterval(timeout);
	timer.setSingleShot(true);

	loop.connect(sender, signal, SLOT(quit()));
	loop.connect(sender, signal2, SLOT(quit()));
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
	strategies::StrategyInterface* cur_str = qobject_cast<strategies::StrategyInterface*>(sender());
	QVERIFY(cur_str);
	if (cur_str)
	{
		qWarning() << cur_str->name() << "onSearchFailed:" << error;
	}
}

void extractHelper::onLinksExtracted(const QMap<int, LinkInfo>& directLinks, int preferredResolutionId)
{
	qDebug() << __FUNCTION__;
	if (!directLinks.isEmpty())
	{
		hasLinks = true;
	}

}

/********************** Tests strategies ****************************/
template<typename strategy_type>
void Test_Strategies<strategy_type>::initTestCase()
{
}

template<typename strategy_type>
void Test_Strategies<strategy_type>::init()
{
}

template<typename strategy_type>
void Test_Strategies<strategy_type>::IsSearch()
{
	helper h;
	QVERIFY(QObject::connect(&strategy, SIGNAL(searchResultsFound(QList<SearchResult>)), &h, SLOT(onSearchResultsFound(QList<SearchResult>))));
	QVERIFY(QObject::connect(&strategy, SIGNAL(searchFailed(const QString&)),  &h, SLOT(onSearchFailed(const QString&))));
	QString searchQuery = "girl";
	strategy.search(searchQuery, strategies::kSortDateAdded, 10, 1);
	bool is_ready_request = ::detail::waitForSignal(&strategy, SIGNAL(searchResultsFound(QList<SearchResult>)), SIGNAL(searchFailed(const QString&)), 30000);
	qDebug() << "Search" << searchQuery << "on" << strategy.name() << "| result size:"  << h.result.size() << "| is_ready_request:" << is_ready_request;
	QVERIFY2(h.result.size() > 0, strategy.name().toLatin1() + ": nothing found");
	std::transform(h.result.begin(), h.result.end(), std::back_inserter(urlsToExtract), [](const SearchResult & r) { return r.originalUrl; });
}

template<typename strategy_type>
void Test_Strategies<strategy_type>::IsExtract()
{
	extractHelper h;
	auto ext = std::shared_ptr<LinkExtractorInterface>(strategy.linkExtractor());
	QVERIFY(QObject::connect(ext.get(), SIGNAL(linksExtracted(const QMap<int, LinkInfo>&, int)), &h, SLOT(onLinksExtracted(const QMap<int, LinkInfo>&, int))));

	Q_FOREACH(QString urlToExtract, urlsToExtract)
	{
		if (!urlToExtract.isEmpty())
		{
			ext->extractLinks(urlToExtract);
			bool is_ready_request = ::detail::waitForSignal(ext.get(), SIGNAL(extractFailed()), SIGNAL(linksExtracted(const QMap<int, LinkInfo>&, int)), 30000);
			QVERIFY(is_ready_request);
			QVERIFY(h.hasLinks);
		}
	}
}

template<typename strategy_type>
void Test_Strategies<strategy_type>::constructor()
{
	QVERIFY(&strategy);
	qDebug() << strategy.name();
}

template<typename strategy_type>
void Test_Strategies<strategy_type>::cleanup()
{

}

template<typename strategy_type>
void Test_Strategies<strategy_type>::cleanupTestCase()
{

}

#define CONCAT(A,B) A##B
#define CONCAT2(A,B) CONCAT(A,B)
#define StrategyClassName CONCAT2(STRATEGY_NAME, Strategy)

#include MOC_FILE
QTEST_MAIN(Test_Strategies<StrategyClassName>)

