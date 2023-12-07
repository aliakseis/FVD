import requests
import json
from datetime import datetime

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

import logging
import traceback


# In the Python version, we're using the requests module for making HTTP requests, and we've also used Python's json module to handle JSON data. 
# Additionally, I've used f-strings (formatted string literals) to construct the URL in a concise manner.

def order_to_string(order):
    # Mapping numeric order values to descriptive strings
    order_map = {
        0: "relevance",
        1: "visited-hour",
        2: "visited"
    }
    return order_map.get(order, "relevance")  # default to "relevance" if order is not found

def Dailymotion_search(query, order, search_limit, page, strategy):
    entities = []

    try:
        page = 1 + (page - 1) * search_limit
        url = f"https://api.dailymotion.com/videos?limit={search_limit}&family_filter=false&fields=id,title,description,duration,url,views_total,thumbnail_medium_url,created_time&sort={order_to_string(order)}&search={query}&page={page}"
        response = requests.get(url)
        data = response.json()
        
        for entry in data["list"]:
            entity = {
                "title": entry["title"],
                "url": entry["url"],
                "published": datetime.utcfromtimestamp(entry["created_time"]),
                "duration": entry["duration"],
                "description": entry["description"],
                "id": entry["id"],
                "viewCount": entry["views_total"],
                "thumbnailUrl": entry["thumbnail_medium_url"]
            }
            entities.append(entity)
    except Exception as e:
        print(e)
        # Add your error handling or logging logic here

    strategy.onSearchFinished(entities)

def Dailymotion_extractDirectLinks(link, receiver) :
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
            if x['vcodec'].startswith('av01'):
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
