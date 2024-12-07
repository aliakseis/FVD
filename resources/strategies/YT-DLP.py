import sys, socket

def install_and_import(package, url=None):
    import importlib
    import subprocess
    import os
    import sys

    if url is None:
        url = package

    def find_pip():
        possible_pip_paths = [
            os.path.join(sys.prefix, 'bin', 'pip3'),
            os.path.join(sys.prefix, 'bin', 'pip3.exe'),
            os.path.join(os.path.dirname(sys.executable), 'pip3'),
            os.path.join(os.path.dirname(sys.executable), 'pip3.exe'),
            os.path.dirname(os.path.abspath(socket.__file__)) + '/../scripts/pip3',
            os.path.dirname(os.path.abspath(socket.__file__)) + '/../scripts/pip3.exe',
            "pip3",
            "pip3.exe",
            "/usr/local/bin/pip3",
            "/usr/local/bin/pip3.exe",
            "/usr/bin/pip3",
            "/usr/bin/pip3.exe"
        ]
        for pip_path in possible_pip_paths:
            if os.path.exists(pip_path):
                return pip_path
        raise FileNotFoundError("pip3 not found in common locations")

    try:
        importlib.import_module(package)
    except ImportError:
        pip_path = find_pip()
        subprocess.run([pip_path, "install", url])
    finally:
        globals()[package] = importlib.import_module(package)

install_and_import("yt_dlp", "yt-dlp") # whichever good

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
            if 'audio_channels' in x and x['audio_channels'] is None:
                continue
            if 'vcodec' in x and x['vcodec'].startswith('av01'):
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
