#pragma once

#include <QString>
#include <set>

struct IsLessCaseInsensitive
{
	bool operator()(const QString& left, const QString& right) const
	{
		return QString::compare(left, right, Qt::CaseInsensitive) < 0;
	}
};

typedef std::set < QString
#ifdef Q_OS_WIN
, IsLessCaseInsensitive
#endif
> EntityFileNames;
