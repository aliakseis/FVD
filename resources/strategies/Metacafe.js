// Metacafe

var LinkExtractor = function()
{
    this._videoId = "";

    this.extractLinks = function(link, receiver) {
        var preferId = 0;
        var dlinks = {};

        try {
            var html = ext.httpRequest(link);
            var pos1 = html.indexOf("flashVarsCache = ");
            var pos2 = html.indexOf(";", pos1);
            if (pos1 > 0 && pos2 > 0) {
                var jsonraw = html.substring(pos1 + 17, pos2 - 1);
                var rx = /mediaData":"([%\w.]*)/;
                var mediaData = decodeURIComponent(rx.exec(jsonraw)[1]);
                var json = JSON.parse(mediaData);
                var i = 0;
                for(var videoData in json) {
                    print(videoData + " " + json[videoData].mediaURL);
                    dlinks[i] = {"url": json[videoData].mediaURL, "extension": "mp4", "resolution": videoData};
                    if(videoData.indexOf("highDefinition") !== -1) {
                        preferId = i;
                    }
                    i++;
                }
            }
        } catch(e) {
            print(e);
            debugger;
        }

        receiver.onlinksExtracted(dlinks, preferId);
    };
}

function extractDirectLinks(link, receiver)
{
    var extractor = new LinkExtractor();
    extractor.extractLinks(link, receiver);
}

function orderToString(order)
{
    switch(order)
    {
    case 0:
        return "rating";
    case 1:
        return "updated";
    case 2:
        return "viewCount";
    }
}
var pageStartInds = [0];
var currentPage = 0;
var searchLimit = 0;
var searchLimitRate = 2;

function setCurrentPage(page)
{
    currentPage = page - 1;

    // add page index
//    var pagesSavedCount = pageStartInds.length;

//    if (pagesSavedCount < page) // add page start index
//    {
//        print("page start index appeared not ready for page " + page + " searchLimit " + searchLimit);
//        if (pagesSavedCount === page - 1)
//        {
//            pageStartInds[pagesSavedCount] = pageStartInds[pagesSavedCount - 1] + searchLimit;
//            print("pageStartInds[pagesSavedCount]" + pageStartInds[pagesSavedCount] + " pageSavedCount=" + pagesSavedCount);
//        }
//    }
//    else if (pagesSavedCount > page)
//    {
//        //pageStartInds = [0];
//    }
}

function setSearchLimit(limit)
{
    if(limit !== searchLimit) {
        pageStartInds = [0];
        for(var i = 0; i < pageStartInds.length; i++) {
            pageStartInds[i] = i * searchLimit;
        }
    }

    searchLimit = limit;
}

function tagText(xml, tagName, startIndex, limitIndex)
{
    var result = "";
    var tagBegin = xml.indexOf("<"+tagName, startIndex);
    var tagEnd = xml.indexOf("</"+tagName, startIndex);
    if(tagBegin > 0  && tagEnd > 0 && tagBegin < limitIndex) {
        result = xml.substring(tagBegin + 2 + tagName.length, tagEnd);
    }
    return result;
}

function search(query, order, searchLimit_, page, strategy)
{
    var entities = [];
    try {
        setSearchLimit(searchLimit_);
        setCurrentPage(page);

        var s = "http://www.metacafe.com/api/videos/?vq=%1&max-results=%2&orderby=%3&start-index=%4";
        s = s.replace("%1", query);
        s = s.replace("%2", searchLimit * searchLimitRate);
        s = s.replace("%3", orderToString(order));
        s = s.replace("%4", pageStartInds[currentPage] + 1);
        var xml = ext.httpRequest(s);

        var itemOpenTag = xml.indexOf("<item>");
        var itemCloseTag = xml.indexOf("</item>");
        var i = 0;
        var skipCount = 0;

        while(itemOpenTag !== -1 && itemCloseTag !== -1 && i < searchLimit) {
            var item = xml.substring(itemOpenTag + 6, itemCloseTag - 7);

            var title = tagText(xml, "media:title", itemOpenTag, itemCloseTag);

            var entity = {};
            entity["title"] = tagText(xml, "media:title", itemOpenTag, itemCloseTag).replace(/&quot;/g, "\"").replace(/&amp;/g, "&");
            entity["url"] = tagText(xml, "link", itemOpenTag, itemCloseTag);
            var datePublishedText = tagText(xml, "pubDate", itemOpenTag, itemCloseTag);
            var date = new Date(datePublishedText);
            entity["published"] = date;
            entity["description"] = tagText(xml, "media:description", itemOpenTag, itemCloseTag);
            entity["id"] = tagText(xml, "id", itemOpenTag, itemCloseTag);

            try {
                entity["duration"] = /duration=\"([\d\w]*)/.exec(item)[1];
                entity["viewCount"] = /(\d+)(?:\s*)views/.exec(item)[1];
            } catch(e) {
                print("Metacafe exception:" + e.message);
                entity["duration"] = 0;
                entity["viewCount"] = 0;
            }

            entity["thumbnailUrl"] = /media:thumbnail\s*url="(.*)(?:")/.exec(item)[1];

            if(/^\d/.test(entity["id"])) {
                entities[i] = entity;
                i++;
            } else {
                print("skip element: " + entity["title"]);
                skipCount++;
            }

            itemOpenTag = xml.indexOf("<item>" , itemOpenTag + 13);
            itemCloseTag = xml.indexOf("</item>",  itemCloseTag + 13);
        }

        if(pageStartInds[currentPage+1] === undefined) {
            pageStartInds[currentPage+1] = searchLimit + skipCount;
        }

        //TODO: port this code
        // check we did not omit too much records
//        if (results.size() < searchLimit && !elements.isEmpty() && (elements.size() - searchLimit) < skippedRecordsCounter)
//        {
//            float oldslRate = searchLimitRate;
//            searchLimitRate = (elements.size() + skippedRecordsCounter + 1) / (1.f * (results.size() + 1));
//            qWarning() << __FUNCTION__ << " omitted too much search results, increasing rate from " << oldslRate << " to " << searchLimitRate;
//        }

    } catch(e) {
        print("Metacafe exception:" + e.message);
        debugger;
    }

    strategy.onSearchFinished(entities);
}
