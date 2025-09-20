#pragma once

#include <QSettings>

#include "strategiescommon.h"

namespace app_settings
{

const char LoggingEnabled[] = "LoggingEnabled";

const char ln[] = "ln";
const char ln_Default[] = "en";

const char IsTrafficLimited[] = "IsTrafficLimited";
const bool IsTrafficLimited_Default = false;

const char TrafficLimitKbs[] = "TrafficLimitKbs";
const int TrafficLimitKbs_Default = 1000;

const char VideoFolder[] = "VideoFolder";

const char Sites[] = "Sites";
const char Sites_Default[] = "YouTube";

const char CheckedSites[] = "CheckedSites";
const char CheckedSites_Default[] = "YouTube";

const char maximumNumberLoads[] = "maximumNumberLoads";
const int maximumNumberLoads_Default = 10;

const char resultsOnPage[] = "resultsOnPage";
const int resultsOnPage_Default = 10;

const char SearchSortOrder[] = "SearchSortOrder";
const int SearchSortOrder_Default = strategies::kSortRelevance;

const char ClearDownloadsOnExit[] = "ClearDownloadsOnExit";
const bool ClearDownloadsOnExit_Default = false;

const char QuitAppWhenClosingWindow[] = "QuitAppWhenClosingWindow";
const bool QuitAppWhenClosingWindow_Default = true;

const char OpenInFolder[] = "OpenInFolder";
const bool OpenInFolder_Default = true;

const char PlayerWidgetOnTheLeft[] = "PlayerWidgetOnTheLeft";
const bool PlayerWidgetOnTheLeft_Default = false;

const char UseProxy[] = "UseProxy";
const bool UseProxy_Default = false;

const char ProxyAddress[] = "ProxyAddress";

const char ProxyPort[] = "ProxyPort";

const char UseProxyAuthorization[] = "UseProxyAuthorization";

const char SiteScripts[] = "YouTube.py;Dailymotion.py;Vimeo.py;YT-DLP.py";

}  // namespace app_settings
