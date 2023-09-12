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


install_and_import("pytube", "https://github.com/pytube/pytube/archive/refs/heads/master.zip") # whichever good

from pytube import Search, YouTube

import logging
import traceback

def YouTube_search(query, order, searchLimit, page, strategy) :
    socket.setdefaulttimeout(30)

    yt_watch = "https://www.youtube.com/watch?v="

    search = Search(query)

    entities = []

    yt_videos= search.results
    if yt_videos is None:
        return
    num = len(yt_videos)
    i = 0
    ii = 0
    while i < num:
        yt_object = yt_videos[i]
        url = yt_watch + yt_object.video_id

        entity = {}

        # https://github.com/pytube/pytube/issues/1674#issuecomment-1706105785
        try:
            yt_object.bypass_age_gate()
        except:
            pass

        try:
            entity["title"] = yt_object.title
            entity["url"] = url
            entity["published"] = str(yt_object.publish_date)
            entity["duration"] = yt_object.length
            entity["description"] = yt_object.description
            entity["id"] = yt_object.video_id
            entity["viewCount"] = yt_object.views
            entity["thumbnailUrl"] = yt_object.thumbnail_url

            entities.append(entity)

            ii += 1
            if ii == searchLimit:
                break

        except:
            logging.warning('Item adding skipped.\n' + traceback.format_exc())

        i += 1
        if i == num:
            i=0
            yt_videos = search.get_next_results()
            if yt_videos is None:
                break
            num = len(yt_videos)

    strategy.onSearchFinished(entities)


def YouTube_extractDirectLinks(link, receiver) :
    socket.setdefaulttimeout(30)

    s=YouTube(link).streams.filter(progressive=True).order_by('resolution').desc()

    entities = {}
    i = 0
    for x in s:
        entity = {}
        entity["url"] = x.url
        entity["resolution"] = x.resolution
        entity["extension"] = x.subtype

        entities[i] = entity

        i += 1

    receiver.onlinksExtracted(entities, 0);
