// Dailymotion

var LinkExtractor = function()
{
    this.extractLinks = function(link, receiver) {
        var preferId = 0;
        var dlinks = {};

        var idRegExp = /\"sequence\"\s*[,:]\s*\"(\S+)\"/i;
        var idRx = /video\/([^_]+)[_&?]/i;
        
        try{
            var idMatch = link.match(idRx);
            ext.clearCookies("dailymotion.com");
            var jsonResponse = ext.httpRequest("http://www.dailymotion.com/sequence/" + idMatch[1]);
            var urlRx = /video_url":"([^"]*)/i;
            var urlMatch = jsonResponse.match(urlRx);
            var url = decodeURIComponent(urlMatch[1]);
            dlinks[preferId] = {"url": url, "extension": "flv", "resolution": "flv"};
        } catch(e) {
            print(e);
            debugger;
        }

        receiver.onlinksExtracted(dlinks, preferId);
    };
}

function Dailymotion_extractDirectLinks(link, receiver)
{
    var cookies = {};
    cookies["family_filter"] = "off";
    ext.setCookies(link, cookies);
    var extractor = new LinkExtractor();
    extractor.extractLinks(link, receiver);
}

function orderToString(order)
{
    /*
    switch(order)
    {
    case 0:
        return "relevance";
    case 1:
        return "visited-hour";
    case 2:
        return "visited";
    }
    */
    return "relevance"; // only supported sort parameter
}

function Dailymotion_search(query, order, searchLimit, page, strategy)
{
    var entities = [];

    try {
        page = 1 + (page - 1) * searchLimit;
        var s = "https://api.dailymotion.com/videos?limit=%1&family_filter=false&fields=id,title,description,duration,url,views_total,thumbnail_medium_url,created_time&sort=%2&search=%3&page=%4";
        s = s.replace("%1", searchLimit);
        s = s.replace("%2", orderToString(order));
        s = s.replace("%3", query);
        s = s.replace("%4", page);
        var res = ext.httpRequest(s);
        var js = JSON.parse(res);
        for(var i = 0; i < js.list.length; i++) {
            var entity = {};
            var entry = js.list[i];
            entity["title"] = entry.title;
            entity["url"] = entry.url;
            entity["published"] = new Date(entry.created_time * 1000);
            entity["duration"] = entry.duration;
            entity["description"] = entry.description;
            entity["id"] = entry.id;
            entity["viewCount"] = entry.views_total;
            entity["thumbnailUrl"] = entry.thumbnail_medium_url;
            entities[i] = entity;
        }
    } catch(e) {
        print(e);
        debugger;
    }

    strategy.onSearchFinished(entities);
}
