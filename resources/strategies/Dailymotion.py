import requests
import json
from datetime import datetime

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
    """
    Search for videos on Dailymotion and return a list of entities representing the search results.

    Args:
    query (str): The search query string.
    order (str): The order in which to return results.
    searchLimit (int): The maximum number of results to return.
    page (int): The page number of the search results to return.
    strategy (object): An object that has an `onSearchFinished` method.

    Returns:
    None: This function does not return a value. Instead, it calls `strategy.onSearchFinished(entities)`.

    This function searches Dailymotion for videos matching the specified query. It constructs a search URL, 
    makes a request to Dailymotion, and parses the JSON response to build a list of entities representing the search results. 
    Each entity contains details about a video, such as its title, URL, publish date, duration, description, view count, 
    and thumbnail URL. The search stops when the specified search limit is reached or when there are no more results. 
    The `strategy.onSearchFinished` method is called with the list of entities once the search is complete.
    """

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
