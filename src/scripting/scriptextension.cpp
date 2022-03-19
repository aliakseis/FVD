#include "scriptextension.h"

#include <QDebug>
#include <QNetworkRequest>
#include <QByteArray>
#include <QCryptographicHash>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include "strategiescommon.h"
#include "utilities/utils.h"

enum { REDIRECT_LIMIT = 10 };

ScriptExtension::ScriptExtension(QObject* parent) :
	QObject(parent),
	m_redirectCount(0)
{
}

ScriptExtension::~ScriptExtension()
{
	qDebug() << __FUNCTION__;
}

QNetworkReply* ScriptExtension::doSyncRequest(const QString& url, QVariantMap const& headers)
{
	QNetworkRequest request(url);

	for (auto it = headers.begin(); it != headers.end(); ++it)
	{
		request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
	}

	QNetworkReply* reply = TheQNetworkAccessManager::Instance().get(request);
	reply->ignoreSslErrors();

	VERIFY(connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onNetworkError(QNetworkReply::NetworkError))));

	return reply;
}

QString ScriptExtension::httpRequest(const QString& url, const QVariantMap& headers)
{
	qDebug() << "httpRequest" << url;
	m_redirectCount = 0;
	QNetworkReply* reply = doSyncRequest(url, headers);
	VERIFY(connect(reply, SIGNAL(finished()), this, SLOT(onHttpRequestFinished())));
	m_requrestWaitLoop.exec();
	return m_syncResult;
}

QNetworkReply* ScriptExtension::httpRequestLowApi(const QString& url, const QVariantMap& headers)
{
	QNetworkReply* reply = doSyncRequest(url, headers);
	VERIFY(connect(reply, SIGNAL(finished()), &m_requrestWaitLoop, SLOT(quit())));
	m_requrestWaitLoop.exec();
	reply->deleteLater();
	return reply;
}

QDateTime ScriptExtension::parseDateString(const QString& rawDateTime, const QString& format)
{
	return QDateTime::fromString(rawDateTime, format);
}

void ScriptExtension::setCookies(const QString& url, QVariantMap cookies)
{
	QNetworkCookieJar* jar = TheQNetworkAccessManager::Instance().cookieJar();
	if (jar != nullptr && !cookies.isEmpty())
	{
		for (auto it = cookies.begin(); it != cookies.end(); ++it)
		{
			qDebug() << it.key() << it.value();
			QList<QNetworkCookie> cookieList;
			cookieList.append(QNetworkCookie(it.key().toLatin1(), it.value().toString().toLatin1()));
			jar->setCookiesFromUrl(cookieList, url);
		}
	}
}

struct CookieJarPublicMorozov : public QNetworkCookieJar
{
	[[nodiscard]] QList<QNetworkCookie> allCookies() const {return QNetworkCookieJar::allCookies();}
	void setAllCookies(const QList<QNetworkCookie>& cookieList) {QNetworkCookieJar::setAllCookies(cookieList);}
};

void ScriptExtension::clearCookies(const QString& domain)
{
	QList<QNetworkCookie> cookies = static_cast<CookieJarPublicMorozov*>(TheQNetworkAccessManager::Instance().cookieJar())->allCookies();
	cookies.erase(
		std::remove_if(
			cookies.begin(), cookies.end(),
			[domain](const QNetworkCookie & cookie) -> bool {return cookie.domain().contains(domain);}),
		cookies.end());
	static_cast<CookieJarPublicMorozov*>(TheQNetworkAccessManager::Instance().cookieJar())->setAllCookies(cookies);
}

void ScriptExtension::onNetworkError(QNetworkReply::NetworkError error)
{
	qDebug() << "ScriptExtension error:" << error;
}

void ScriptExtension::onHttpRequestFinished()
{
	qDebug() << __FUNCTION__;

	auto* myReply = qobject_cast<QNetworkReply*>(sender());

	QVariant possibleRedirectUrl = myReply->attribute(QNetworkRequest::RedirectionTargetAttribute);

	if (!possibleRedirectUrl.isNull() && m_redirectCount++ < REDIRECT_LIMIT)
	{
		QUrl redirect = possibleRedirectUrl.toUrl();
		QUrl url = myReply->url();
		myReply->deleteLater();
		myReply = redirect.isRelative() ?
				  TheQNetworkAccessManager::Instance().get(QNetworkRequest(url.scheme() + "://" + url.host() + redirect.toString())) :
				  TheQNetworkAccessManager::Instance().get(QNetworkRequest(redirect));

		VERIFY(connect(myReply, SIGNAL(finished()), this, SLOT(onHttpRequestFinished())));
		VERIFY(connect(myReply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onNetworkError(QNetworkReply::NetworkError))));
		return;
	}

	QByteArray ba = myReply->readAll();
	m_syncResult = QString::fromUtf8(ba);
	m_requrestWaitLoop.quit();
	myReply->deleteLater();
	myReply = nullptr;
}

QString ScriptExtension::hmac_sha1(const QString& text, const QString& secret)
{
	int secret_length = secret.size();
	QByteArray inner_padding;
	QByteArray outer_padding;
	QByteArray hash_key = secret.toLatin1();

	// Obtaining sha1 key
	if (secret_length > 64)
	{
		QByteArray temp_secret;
		temp_secret.append(secret);
		hash_key = QCryptographicHash::hash(temp_secret, QCryptographicHash::Sha1);
		secret_length = 20;
	}

	inner_padding.fill(0, 64);
	outer_padding.fill(0, 64);

	inner_padding.replace(0, secret_length, hash_key);
	outer_padding.replace(0, secret_length, hash_key);

	for (int i = 0; i < 64; ++i)
	{
		inner_padding[i] = inner_padding[i] ^ 0x36;
		outer_padding[i] = outer_padding[i] ^ 0x5c;
	}

	QByteArray context;
	context.append(inner_padding, 64);
	context.append(text);

	//Hashes Inner pad
	QByteArray Sha1 = QCryptographicHash::hash(context, QCryptographicHash::Sha1);

	context.clear();
	context.append(outer_padding, 64);
	context.append(Sha1);

	Sha1.clear();
	Sha1 = QCryptographicHash::hash(context, QCryptographicHash::Sha1);

	return Sha1.toBase64();
}

