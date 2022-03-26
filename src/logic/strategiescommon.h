#pragma once

#include <QNetworkAccessManager>

#include "utilities/singleton.h"

typedef Singleton<QNetworkAccessManager> TheQNetworkAccessManager;

namespace strategies
{
enum SortOrder
{
    kSortRelevance,
    kSortDateAdded,
    kSortViewCount,

    MAX_SortOrder
};
}
