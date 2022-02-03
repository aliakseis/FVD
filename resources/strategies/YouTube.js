var FORMAT_LABEL={'18':'MP4 360p','22':'MP4 720p (HD)','34':'FLV 360p','35':'FLV 480p','37':'MP4 1080p (HD)','38':'MP4 4K (HD)','43':'WebM 360p','44':'WebM 480p','45':'WebM 720p (HD)','46':'WebM 1080p (HD)'};
var FORMAT_TYPE={'18':'mp4','22':'mp4','34':'flv','35':'flv','37':'mp4','38':'mp4','43':'webm','44':'webm','45':'webm','46':'webm'};
var FORMAT_ORDER=['18','34','43','35','44','22','45','37','46','38'];
var FORMAT_RULE={'flv':'max','mp4':'all','webm':'none'};


var ls_STORAGE_CODE, ls_STORAGE_URL = null;
var videoID, videoTicket, videoFormats, scriptURL=null;
var preferId = 0;
var dlinks = {};


var LinkExtractor = function () {
 function isInteger(n) {
	 return (typeof n==='number' && n%1==0);
  }

  function isString(s) {
	return (typeof s==='string'|| s instanceof String);
  }
	function decryptSignature(sig) {
		print('decryptSignature()');
		function swap(a,b){var c=a[0];a[0]=a[b%a.length];a[b]=c;return a};
		function decode(sig, arr) { // encoded decryption
		  if (!isString(sig)) return null;
		  var sigA=sig.split('');
		  for (var i=0;i<arr.length;i++) {
			var act=arr[i];
			if (!isInteger(act)) return null;
			sigA=(act>0)?swap(sigA, act):((act==0)?sigA.reverse():sigA.slice(-act));
		  }
		  var result=sigA.join('');
		  return (result.length==81)?result:sig;
		}
		
		if (sig==null) {
			return '';
		}

		print('decryptSignature  ls_STORAGE_CODE = ' + ls_STORAGE_CODE);
		if ( ls_STORAGE_CODE &&  ls_STORAGE_CODE!='error') {
		  var arr=ls_STORAGE_CODE;
		  arr=arr.split(',');
		  if (arr!=null){
			for (var i=0; i<arr.length; i++) {
			  arr[i]=parseInt(arr[i], 10);
			}
			var sig2=decode(sig, arr);
			if (sig2!=null && sig2.length==81) return sig2;
		  }
		}



	   var decodeArray={92:[-2,0,-3,9,-3,43,-3,0,23], 88:[-2,1,10,0,-2,23,-3,15,34],
			87:[-3,0,63,-2,0,-1], 85:[0,-2,17,61,0,-1,7,-1], 83:[24,53,-2,31,4], 
			81:[34,29,9,0,39,24]};
		var arr=decodeArray[sig.length];    
		if (arr!=null) {
		  sig=decode(sig, arr);
		}
		return sig; 
	}

function updateSignatureCode(scriptURL) {
	print('updateSignatureCode() scriptURL - ' + scriptURL );
try {
	  function isValidArray(arr) {
		if (arr==null) return false;
		if (arr=='error') return true;
		arr=arr.split(',');
		if (arr.length<=2) return false;
		for (var i=0;i<arr.length;i++) {
		  var act=parseInt(arr[i],10);
		  if (!isInteger(act)) return false;
		}
		return true;
	  }
	   
	var storageURL=ls_STORAGE_URL;
	if (isValidArray(ls_STORAGE_CODE) && storageURL!=null &&
	  scriptURL.replace(/^https?/i,'')==storageURL.replace(/^https?/i,'')) 
	{
		print('updateSignatureCode ls_STORAGE_URL = ' + ls_STORAGE_URL + '  ls_STORAGE_CODE = ' + ls_STORAGE_CODE);
		print('updateSignatureCode() befor all  return;');
		return;
	}
	} catch (e) {
		print('--- updateSignatureCode  Exception: ' + e);
		debugger;
	}

	print('updateSignatureCode()  - HttpSendForGetContext()  ***************************************************************** +++++++ ');
	//globalObj.HttpSendForGetContext(scriptURL, 'set_return_code');
	updateSignatureCodeNEXT();
	getDirectLinc();
   }

function updateSignatureCodeNEXT() {
	try {
	  print('updateSignatureCodeNEXT() +++++++ ');
	  var decodeArray=[];
	  var sourceCode=ext.httpRequest(scriptURL);
	  var functionNameMatches=sourceCode.match(/b\.signature=(\w+)\(/);
	  functionName=(functionNameMatches)?functionNameMatches[1]:null;
	  if (functionName==null) {
		ls_STORAGE_CODE="error";
		return;
	  }
	  var regCode=new RegExp('function '+functionName+
	  '\\s*\\(\\w+\\)\\s*{\\w+=\\w+\\.split\\(""\\);(.+);return \\w+\\.join');
	  var functionCodeMatches=sourceCode.match(regCode);
	  functionCode=(functionCodeMatches)?functionCodeMatches[1]:null;
	  if (functionCode==null) {
		ls_STORAGE_CODE="error";
		return;
	  }
	  var regSlice=new RegExp('slice\\s*\\(\\s*(.+)\\s*\\)');
	  var regSwap=new RegExp('\\w+\\s*\\(\\s*\\w+\\s*,\\s*([0-9]+)\\s*\\)');
	  var regInline=new RegExp('\\w+\\[0\\]\\s*=\\s*\\w+\\[([0-9]+)\\s*%\\s*\\w+\\.length\\]');              
	  var functionCodePieces = functionCode.split(';');
	  for (var i = 0; i < functionCodePieces.length; i++) {
		functionCodePieces[i]=functionCodePieces[i].trim();
		if (functionCodePieces[i].length==0) {
		} else if (functionCodePieces[i].indexOf("slice") >= 0) { // slice
		  var sliceMatches=functionCodePieces[i].match(regSlice);
		  var slice=(sliceMatches)?sliceMatches[1]:null;
		  slice=parseInt(slice, 10);
		  if (isInteger(slice)){ 
			decodeArray.push(-slice);
		  } else {
			ls_STORAGE_CODE="error";
			return;
		  }
		} else if (functionCodePieces[i].indexOf("reverse") >= 0) {
		  decodeArray.push(0);
		} else if (functionCodePieces[i].indexOf("[0]") >= 0) {
			if (i+2<functionCodePieces.length &&
			functionCodePieces[i+1].indexOf(".length") >= 0 &&
			functionCodePieces[i+1].indexOf("[0]") >= 0) {
			  var inlineMatches=functionCodePieces[i+1].match(regInline);
			  var inline=(inlineMatches)?inlineMatches[1]:null;
			  inline=parseInt(inline, 10);
			  decodeArray.push(inline);
			  i=i+2;
			} else {
			  ls_STORAGE_CODE="error";
			  return;
			}
		} else if (functionCodePieces[i].indexOf(",") >= 0) {
		  var swapMatches=functionCodePieces[i].match(regSwap);
		  var swap=(swapMatches)?swapMatches[1]:null;
		  swap=parseInt(swap, 10);
		  if (isInteger(swap)){ 
			decodeArray.push(swap);
		  } else {
			ls_STORAGE_CODE="error";
			return;
		  }                                    
		} else {
		  ls_STORAGE_CODE="error";
		  return;
		}                
	  }
	  
	  ls_STORAGE_URL=scriptURL;
	  ls_STORAGE_CODE=decodeArray.toString();
	  print('updateSignatureCodeNEXT ls_STORAGE_URL = ' + ls_STORAGE_URL + '  ls_STORAGE_CODE = ' + ls_STORAGE_CODE);

	} catch (e) {
		print(' #############################   updateSignatureCodeNEXT Exception: ' + e);
	}
}

function getDirectLinc(){

try {
	print('getDirectLinc()');

	// parse the formats map
	var sep1 = '%2C', sep2 = '%26', sep3 = '%3D';
	if (videoFormats.indexOf(',') > -1) {
	  sep1 = ',';
	  sep2 = (videoFormats.indexOf('&') > -1) ? '&' : '\\u0026';
	  sep3 = '=';
	}

	var videoURL = new Array();
	var videoFormatsGroup = videoFormats.split(sep1);
	for (var i=0;i<videoFormatsGroup.length;i++) {
		var videoFormatsElem=videoFormatsGroup[i].split(sep2);
		var videoFormatsPair=new Array();
		for (var j=0;j<videoFormatsElem.length;j++) {
		  var pair=videoFormatsElem[j].split(sep3);
		  if (pair.length==2) {
			videoFormatsPair[pair[0]]=pair[1];
		  }
		}
		if (videoFormatsPair['url']==null) continue;
		url=unescape(unescape(videoFormatsPair['url'])).replace(/\\\//g,'/').replace(/\\u0026/g,'&');
		if (videoFormatsPair['itag']==null) continue;
		itag=videoFormatsPair['itag'];
		if (videoFormatsPair['sig']) {
		  url=url+'&signature='+videoFormatsPair['sig'];
		} else if (videoFormatsPair['s']) {
		  url=url+'&signature='+decryptSignature(videoFormatsPair['s']);
		}
		if (url.toLowerCase().indexOf('http')==0) { // validate URL
		  videoURL[itag]=url; 
		}
	}

	var showFormat = new Array();
	for (var category in FORMAT_RULE) {
	  var rule = FORMAT_RULE[category];
	  for (var index in FORMAT_TYPE) {
		if (FORMAT_TYPE[index] == category) {
		  showFormat[index] = (rule == 'all');
		}
	  }
	  if (rule == 'max') {
		for (var i = FORMAT_ORDER.length - 1; i >= 0; i--) {
		  var format = FORMAT_ORDER[i];
		  if (FORMAT_TYPE[format] == category && videoURL[format] != undefined) {
			showFormat[format] = true;
			break;
		  }
		}
	  }
	}

	for (var i = 0; i < FORMAT_ORDER.length; i++) {
	  var format = FORMAT_ORDER[i];
	  if (videoURL[format] != undefined && FORMAT_LABEL[format] != undefined && showFormat[format]) {
		//var url = videoURL[format];
		dlinks[format] = { "url": videoURL[format], "extension": FORMAT_TYPE[format], "resolution": FORMAT_LABEL[format] };
		preferId = format;
	  }
	}

  } catch (e) {
	print('---- getDirectLinc Exception: ' + e);
  }
}



this.extractLinks = function (link, receiver) {

try {

		var requestUrl = link;
		ext.clearCookies("youtube.com");
		var bodyContent = ext.httpRequest(requestUrl);
	
		if (videoID==null || videoTicket==null) { // if all else fails
			if (bodyContent!=null) {
			  var videoIDMatches=bodyContent.match(/\"video_id\":\s*\"([^\"]+)\"/);
			  videoID=(videoIDMatches)?videoIDMatches[1]:null;
			  var videoTicketMatches=bodyContent.match(/\"t\":\s*\"([^\"]+)\"/);
			  videoTicket=(videoTicketMatches)?videoTicketMatches[1]:null;
			  var videoFormatsMatches=bodyContent.match(/\"url_encoded_fmt_stream_map\":\s*\"([^\"]+)\"/);
			  videoFormats=(videoFormatsMatches)?videoFormatsMatches[1]:null;
			  if (scriptURL==null) {
				var scriptURLMatches=bodyContent.match(/\"js\":\s*\"([^\"]+)\"/);
				scriptURL=(scriptURLMatches)?scriptURLMatches[1]:null;
				scriptURL=scriptURL.replace(/\\/g,'');
			  }      
			}
		}
//debugger;
		if (videoID==null || videoTicket==null || videoFormats==null || videoID.length==0 || videoTicket.length==0 || videoFormats.length==0) {
		   print('DYVAM - Error: No config information found. YouTube must have changed the code.');
		   return;
		}

		if (scriptURL!=null) {
			updateSignatureCode(scriptURL);
		} else {
		  getDirectLinc();
		}

		receiver.onlinksExtracted(dlinks, preferId);

	} catch (e) {
		print('++++++++++ ###########  Exception: ' + e);
		debugger;
	}

	};
}

function extractDirectLinks(link, receiver) {

ls_STORAGE_CODE= null;
ls_STORAGE_URL = null;

videoID=null;
videoTicket=null;
videoFormats=null;
scriptURL=null;

preferId = 0;
dlinks = {};

	var extractor = new LinkExtractor();
	extractor.extractLinks(link, receiver);
}

function orderToString(order) {
	switch (order) {
		case 0:
			return "relevance";
		case 1:
			return "published";
		case 2:
			return "viewCount";
	}
}

function search(query, order, searchLimit, page, strategy) {
	var entities = [];
	try {
		page = 1 + (page - 1) * searchLimit;
		var s = "http://gdata.youtube.com/feeds/api/videos?v=2&max-results=%1&safeSearch=none&q=%2&orderby=%3&alt=json&start-index=%4";
		s = s.replace("%1", searchLimit);
		s = s.replace("%2", query);
		s = s.replace("%3", orderToString(order));
		s = s.replace("%4", page);
		var httpHeaders = {};
		httpHeaders["GData-Version"] = "2";
		httpHeaders["Cache-Control"] = "no-cache";
		var res = ext.httpRequest(s, httpHeaders);
		var js = JSON.parse(res);

		if (typeof js.feed.entry != 'undefined') {
			for (var i in js.feed.entry) {
				var entity = {};
				var entry = js.feed.entry[i];
				entity["title"] = entry.title["$t"];
				entity["url"] = entry.media$group.media$player.url;
				entity["published"] = entry.media$group.yt$uploaded.$t; //ext.parseDateString(entry.media$group.yt$uploaded.$t, "yyyy-MM-ddTHH:mm:ss.000Z");
				entity["duration"] = entry.media$group.yt$duration.seconds;
				entity["description"] = entry.media$group.media$description.$t;
				entity["id"] = entry.media$group.yt$videoid.$t;
				try {
					entity["viewCount"] = entry.yt$statistics.viewCount;
				} catch (e) {
					entity["viewCount"] = 0;
				}

				if (entry.media$group.media$thumbnail.length > 1)
					entity["thumbnailUrl"] = entry.media$group.media$thumbnail[1].url;
				else
					entity["thumbnailUrl"] = entry.media$group.media$thumbnail[0].url;

				entities[i] = entity;
			}
		} else {
			print('cannot parse ' + res);
			debugger;
		}
	} catch (e) {
		print(e);
		debugger;
	}

	strategy.onSearchFinished(entities);
}
