import sys, socket

def install_and_import(package, url):
    import importlib
    try:
        importlib.import_module(package)
    except ImportError:
        import subprocess
        subprocess.run(["pip3", "install", url])
    finally:
        globals()[package] = importlib.import_module(package)


install_and_import("yt_dlp", "yt-dlp") # whichever good

from pytube import Search, YouTube

import logging
import traceback

def YT_DLP_search(query, order, searchLimit, page, strategy) :
    socket.setdefaulttimeout(30)

    ydl_opts = {
	    'format': 'best',
	    'outtmpl': '%(title)s.%(ext)s'
    }

    search = yt_dlp.YoutubeDL(ydl_opts).extract_info(f'ytsearch{searchLimit}:{query}', download=False)

    entities = []

    yt_videos= search['entries']
    if yt_videos is None:
        return
    ii = 0
    for yt_object in yt_videos:
        entity = {}

        try:
            entity["title"] = yt_object["fulltitle"]
            entity["url"] = yt_object["webpage_url"]
            entity["published"] = yt_object["upload_date"]
            entity["duration"] = yt_object["duration"]
            entity["description"] = yt_object["description"]
            entity["id"] = yt_object["id"]
            entity["viewCount"] = yt_object["view_count"]
            entity["thumbnailUrl"] = yt_object["thumbnail"]

            entities.append(entity)

            ii += 1
            if ii == searchLimit:
                break

        except:
            logging.warning('Item adding skipped.\n' + traceback.format_exc())

    strategy.onSearchFinished(entities)


def YT_DLP_extractDirectLinks(link, receiver) :
    socket.setdefaulttimeout(30)

    ydl_opts = {
        'format': 'best',
        'get-url': True,
    }

    s = yt_dlp.YoutubeDL(ydl_opts).extract_info(link, download=False)['formats']

    entities = {}
    i = 0
    height = 0
    best = 0

    for x in s:
        entity = {}
        try:
            if x['protocol'] == 'm3u8_native':
                continue
            if x['audio_channels'] is None:
                continue

            entity["url"] = x["url"]
            entity["resolution"] = x["resolution"]
            entity["extension"] = x["ext"]

            if x["height"] > height:
                height = x["height"]
                best = i

            entities[i] = entity

            i += 1
        except:
            logging.warning('Item adding skipped.\n' + traceback.format_exc())

    receiver.onlinksExtracted(entities, best);
