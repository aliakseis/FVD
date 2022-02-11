#include "translation.h"

#include <QtGlobal>
#include <QString>
#include <QVariant>
#include <QCoreApplication>
#include <QDebug>
#include <QTranslator>
#include <QRegExp>

namespace utilities
{

namespace Tr
{


QString TranslationRule::GetText() const
{
    return expand_.isEmpty() ? Tr(translateArgs_) : multiArg(Tr(translateArgs_), expand_.size(), expand_.data());
}

///////////////////////////////////////////////////////////////////////////////


Translation translate(const char* context, const char* sourceText, const char* disambiguation, int n)
{
    return { context, sourceText, disambiguation, n };
}

QString Tr(const Translation& translateArgs)
{
    return QCoreApplication::translate(
            translateArgs.context, translateArgs.key, translateArgs.disambiguation,
            translateArgs.n);
}

Translation Plural(const Translation& translateArgs, int n)
{
    Translation plural = translateArgs;
    plural.n = n;
    return plural;
}

void Retranslate(QObject* obj)
{
    if (!obj) { return; }
    QVariant variant = obj->property(kProperty);
    if (variant.canConvert<TranslationRules>())
    {
        TranslationRules rules = variant.value<TranslationRules>();
        for (auto it = rules.begin(); it != rules.end(); ++it)
        {
            it.key()->execute(it.value()->GetText());
        }
    }
}

void RetranslateAll(QObject* obj)
{
    if (!obj) { return; }
    Retranslate(obj);
    auto objects = obj->findChildren<QObject*>();
    for (auto o : qAsConst(objects))
    {
        RetranslateAll(o);
    }
}

} // namespace Tr


} // namespace utilities
