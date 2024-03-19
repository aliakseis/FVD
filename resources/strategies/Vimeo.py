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


install_and_import("vimeo_downloader", "git+https://github.com/yashrathi-git/vimeo_downloader") # whichever good

from vimeo_downloader import Vimeo

from requests import get as request
from urllib.parse import quote_plus
from json import loads

import logging
import traceback

def Vimeo_search(query, order, searchLimit, page, strategy):
    """
    Search for videos on Vimeo and return a list of entities representing the search results.

    Args:
    query (str): The search query string.
    order (str): The order in which to return results.
    searchLimit (int): The maximum number of results to return.
    page (int): The page number of the search results to return.
    strategy (object): An object that has an `onSearchFinished` method.

    Returns:
    None: This function does not return a value. Instead, it calls `strategy.onSearchFinished(entities)`.

    Raises:
    Warning: If the request to Vimeo fails or the JSON response cannot be parsed.

    This function searches Vimeo for videos matching the specified query. It constructs a search URL, 
    makes a request to Vimeo, and parses the JSON response to build a list of entities representing the search results. 
    Each entity contains details about a video, such as its title, URL, publish date, duration, description, view count, 
    and thumbnail URL. The search stops when the specified search limit is reached or when there are no more results. 
    The `strategy.onSearchFinished` method is called with the list of entities once the search is complete.
    """

    socket.setdefaulttimeout(30)

    encoded_search = quote_plus(query)

    url = f"https://vimeo.com/search?q={encoded_search}"

    entities = []

    i = 1
    ii = 0

    headers = {'user-agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.87 Safari/537.36'}

    stop = False
    while not stop:
        try:
            response = request(url, timeout=10, headers=headers).text
            index1 = response.index("vimeo.config = ")
            index1 = response.index("[", index1)
            index2 = response.index("}}}]", index1) + 4
            text = response[index1:index2]
        except: 
            logging.warning('Failed to get Vimeo info.\n' + traceback.format_exc())
            break

        if not text:
            break

        try:
            dicti = loads(text)
        except: 
            logging.warning('Failed to parse JSON.\n' + traceback.format_exc())
            break

        if not dicti:
            break

        prev_ii = ii

        for song in dicti:
            entity = {}

            title = song["clip"]["name"]
            link = song["clip"]["link"]
            entity["title"] = title
            entity["url"] = link
            entity["published"] = song["clip"]["created_time"]
            entity["duration"] = song["clip"]["duration"]
            entity["description"] = title + "\n" + link
            entity["id"] = link
            entity["viewCount"] = song["clip"]["stats"]["plays"]
            entity["thumbnailUrl"] = song["clip"]["pictures"]["sizes"][0]["link"]

            entities.append(entity)

            ii += 1
            if ii == searchLimit:
                stop = True
                break

        if ii == prev_ii:
            break

        i += 1
        url = f"https://vimeo.com/search/page:{i}?q={encoded_search}"


    strategy.onSearchFinished(entities)


def Vimeo_extractDirectLinks(link, receiver) :
    socket.setdefaulttimeout(30)

    v = Vimeo(link)

    entities = {}
    i = 0
    s = v.streams
    for x in s:
        entity = {}
        entity["url"] = x.direct_url
        entity["resolution"] = x.quality
        entity["extension"] = "mp4"

        entities[i] = entity

        i += 1

    receiver.onlinksExtracted(entities, i-1);
