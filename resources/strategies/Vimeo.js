// Vimeo

var kAPIKey = "d5b4acb8850a23b52dcb61d2ea9b0649df9e9d79";
var kAPISecret = "c35eac05ff8d5627d64675871d122f25ebcd2c78";

var preferedQualities = {"mobile":0, "sd":1, "hd":2};

var LinkExtractor = function()
{
    this.extractLinks = function(link, receiver) 
    {
        var preferId = 0;
        var qualityId = 0;
        var dlinks = {};
        var idRegExp = /({config:[\S?\s]+});/i;
        try
        {
            var raw = ext.httpRequest(link);
            var matches = raw.match(idRegExp);
            
            if(matches.length > 1) 
            {
                var forParsing = matches[1].replace(/({|,)\s*(\w+)\s*:/g, '$1"$2":').replace(/(:\s+)\'/g, '$1"').replace(/\'([,\]}])/g, '"$1');

                //forParsing.replace(/({|,)\s*(\w+)\s*:/g, '$1"$2"');
                //forParsing.replace(/(:\s+)\'/g, '$1"');
                //forParsing.replace(/\'([,\]}])/g, '"$1');

                var js = JSON.parse(forParsing);

                var videoId = js.config.video.id;
                var timestamp = js.config.request.timestamp;
                var signature = js.config.request.signature;

                var qualities = [];
                for(var i = 0; i < js.config.video.smil.qualities.length; i++) 
                {
                    qualities[i] = js.config.video.smil.qualities[i];
                }
                if(qualities.length == 0)
                {
                    qualities.push("sd&type=moogaloop_local&codecs=H264,VP8,VP6");
                    var hd = js.config.video.hd;
                    if ("1" == hd)
                    {
                        qualities.push("hd&type=moogaloop_local&codecs=H264,VP8,VP6");
                    }

                    for(var i = 0; i < js.config.video.files.h264.length; i++)
                    {
                        var qual = js.config.video.files.h264[i];
                        if (qual != "sd" && qual != "hd")
                        {
                            qualities.push(qual + "&type=moogaloop_local&codecs=H264,VP8,VP6");
                        }
                    }
                }

                var videoUrl = "http://player.vimeo.com/play_redirect?clip_id=%1&sig=%2&time=%3&quality=";
                videoUrl = videoUrl.replace("%1", videoId);   
                videoUrl = videoUrl.replace("%2", signature);
                videoUrl = videoUrl.replace("%3", timestamp);

                for(var i = 0; i < qualities.length; i++)
                {
                    var url = videoUrl + qualities[i];

                    var headers = {};
                    headers["Accept-Charset"] = "ISO-8859-1,utf-8;q=0.7,*;q=0.7";
                    headers["Accept-Language"] = "en-us,en;q=0.5";
                    headers["Connection"] = "close";
                    headers["Host"] = "player.vimeo.com";

                    var reply = ext.httpRequestLowApi(url, headers);
                    var quality = reply.requestQueryItemValue("quality");
                    if(preferedQualities[quality] > qualityId) {
                        preferId = i;
                        qualityId = preferedQualities[quality];
                    }
                    dlinks[i] = {"url": reply.rawHeader("Location"), "extension": "mp4", "resolution": quality};
                }
             }
        } catch(e) {
            print(e);
            debugger;
        }
        receiver.onlinksExtracted(dlinks, preferId);
    }
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
        return "relevant";
    case 1:
        return "newest";
    case 2:
        return "most_played";
    }
}

addGetParam = function(m_getParams, key, value)
{
    m_getParams[key] = value;
}

addOAuthParam = function(m_getParams, m_oauthParams, key, value)
{
    m_getParams[key] = value;
    m_oauthParams[key] = value;
}

function search(query, order, searchLimit, page, strategy)
{
    var baseString = "GET&" + encodeURIComponent("http://vimeo.com/api/rest/v2/");
    var queryStr =  encodeURIComponent(query);

    var m_oauthParams={};
    var m_getParams={};

    
    addGetParam(m_getParams,"format", "json");
    addGetParam(m_getParams,"full_response", "1");
    addGetParam(m_getParams,"method", "vimeo.videos.search");
    addOAuthParam(m_getParams, m_oauthParams, "oauth_consumer_key", kAPIKey);
    var rand = Math.floor((Math.random() * 1000));
    addOAuthParam(m_getParams, m_oauthParams, "oauth_nonce", rand);
    addOAuthParam(m_getParams, m_oauthParams,"oauth_signature_method", "HMAC-SHA1");
    var seconds = Math.floor(new Date().getTime() / 1000);
    addOAuthParam(m_getParams, m_oauthParams, "oauth_timestamp", seconds);
    addOAuthParam(m_getParams, m_oauthParams,"oauth_token", "");
    addOAuthParam(m_getParams, m_oauthParams,"oauth_version", "1.0");
    addGetParam(m_getParams,"page", page);
    addGetParam(m_getParams,"per_page", searchLimit);
    addGetParam(m_getParams,"query", query);
    var sort_string = orderToString(order);
    addGetParam(m_getParams,"sort", sort_string);

    var params = "";
    for(var key in m_getParams) 
    {
        if(params != "")
        {
            params += "&";
        }
        params += key + "="  + m_getParams[key];
    }

    baseString += "&" + encodeURIComponent(params);
    var secret = encodeURIComponent(kAPISecret) + "&";
    var hash = ext.hmac_sha1(baseString, secret);
    var sign = encodeURIComponent(hash);

    var oauth = 'OAuth realm="", ';
    var first = true;
    for(var key in m_oauthParams)
    {
        if (!first)
        {
            oauth += ", ";
        }
        first = false;

        oauth += key + '="' + m_oauthParams[key] + '"';
    }
    oauth += ', oauth_signature="'  + sign + '"';
    var url = "http://vimeo.com/api/rest/v2/?format=json&full_response=1&method=vimeo.videos.search&query=%1&per_page=%2&sort=%3&page=%4";

    url = url.replace("%1", query);
    url = url.replace("%2", searchLimit);
    url = url.replace("%3", sort_string);
    url = url.replace("%4", page);

    var headers = {};
    headers["Authorization"] = oauth;

    var res = ext.httpRequest(url, headers);
    var js = JSON.parse(res);
    var entities = [];

    if (js.videos.video instanceof Array)
    	for (var i = 0; i < js.videos.video.length; ++i) {
    		try {
    			var entity = {};
    			var entry = js.videos.video[i];
    			entity["title"] = entry.title;
    			entity["url"] = "http://vimeo.com/" + entry.id;
    			entity["published"] = entry.upload_date;
    			entity["duration"] = entry.duration;
    			entity["description"] = entry.description;
    			entity["id"] = entry.id;
    			entity["viewCount"] = entry.number_of_plays;

    			if (entry.thumbnails.thumbnail.length > 1)
    				entity["thumbnailUrl"] = entry.thumbnails.thumbnail[1]._content;
    			else
    				entity["thumbnailUrl"] = entry.thumbnails.thumbnail[0]._content;

    			entities[i] = entity;
    		} catch (e) {
    			print(e);
    			debugger;
    		}
    }

    strategy.onSearchFinished(entities);
}
