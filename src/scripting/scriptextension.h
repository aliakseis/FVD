#pragma once

#include <QDateTime>
#include <QEventLoop>
#include <QNetworkReply>
#include <QObject>
#include <QVariant>

class ScriptExtension : public QObject
{
    Q_OBJECT
public:
    explicit ScriptExtension(QObject* parent = 0);
    ~ScriptExtension();

signals:
    void searchFinished(QVariantList list);

public slots:
    QString httpRequest(const QString& url, const QVariantMap& headers = QVariantMap());
    QNetworkReply* httpRequestLowApi(const QString& url, const QVariantMap& headers = QVariantMap());
    static QDateTime parseDateString(const QString& rawDateTime, const QString& format);
    static QString hmac_sha1(const QString& text, const QString& secret);
    static void setCookies(const QString& url, QVariantMap cookies);
    static void clearCookies(const QString& domain);

private slots:
    static void onNetworkError(QNetworkReply::NetworkError);
    void onHttpRequestFinished();

private:
    QEventLoop m_requrestWaitLoop;
    QString m_syncResult;
    int m_redirectCount;

    QNetworkReply* doSyncRequest(const QString& url, QVariantMap const& headers);
};
