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

import re
import json
from typing import Optional, Dict
import requests

HEADERS = {
    "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) "
                  "Chrome/120.0.0.0 Safari/537.36",
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
    "Accept-Language": "en-US,en;q=0.9",
    "Referer": "https://www.youtube.com/",
    "Connection": "keep-alive",
}

def fetch_html(url: str, timeout: int = 10) -> Optional[str]:
    try:
        resp = requests.get(url, headers=HEADERS, timeout=timeout)
        if resp.status_code == 200:
            return resp.text
    except requests.RequestException:
        return None
    return None

def regex_extract_from_html(html: str) -> Dict[str, Optional[str]]:
    """
    Best-effort extraction of description, upload_date (YYYYMMDD) and an i.ytimg.com thumbnail URL.
    Returns dict with keys: description, upload_date, thumbnail
    """
    result = {"description": None, "upload_date": None, "thumbnail": None}

    # 1) JSON-LD
    m = re.search(r'<script[^>]+type=["\']application/ld\+json["\'][^>]*>(.*?)</script>', html, re.S|re.I)
    if m:
        try:
            ld = json.loads(m.group(1).strip())
            if isinstance(ld, list):
                for obj in ld:
                    if obj.get('@type') == 'VideoObject':
                        ld = obj
                        break
            if isinstance(ld, dict):
                desc = ld.get('description')
                thumb = ld.get('thumbnailUrl')
                date = ld.get('uploadDate') or ld.get('datePublished')
                if desc:
                    result['description'] = desc
                if isinstance(thumb, list) and thumb:
                    result['thumbnail'] = thumb[0].split('?')[0]
                elif isinstance(thumb, str):
                    result['thumbnail'] = thumb.split('?')[0]
                if date:
                    result['upload_date'] = date.replace('-', '') if '-' in date else date
                # If we already have both description and date and thumbnail, return.
                if result['description'] and result['upload_date'] and result['thumbnail']:
                    return result
        except Exception:
            pass

    # 2) ytInitialPlayerResponse blob (search and parse minimal JSON)
    m2 = re.search(r'ytInitialPlayerResponse\s*=\s*({.*?})\s*;', html, re.S)
    if m2:
        try:
            blob = json.loads(m2.group(1))
            # description
            sd = blob.get('videoDetails', {}).get('shortDescription')
            if sd and not result['description']:
                result['description'] = sd
            # thumbnails
            thumbs = blob.get('videoDetails', {}).get('thumbnail', {}).get('thumbnails') or []
            if thumbs and not result['thumbnail']:
                # pick largest by width if present
                try:
                    best = max(thumbs, key=lambda t: int(t.get('width') or 0))
                    result['thumbnail'] = best.get('url', '').split('?')[0]
                except Exception:
                    result['thumbnail'] = thumbs[-1].get('url', '').split('?')[0]
            # uploadDate may appear elsewhere as datePublished / uploadDate inside player response
            # try to find any known key in the blob text if not present
            if not result['upload_date']:
                jd = json.dumps(blob)
                mdate = re.search(r'"uploadDate"\s*:\s*"(\d{4}-\d{2}-\d{2})"', jd) or re.search(r'"datePublished"\s*:\s*"(\d{4}-\d{2}-\d{2})"', jd)
                if mdate:
                    result['upload_date'] = mdate.group(1).replace('-', '')
            if result['description'] and result['upload_date'] and result['thumbnail']:
                return result
        except Exception:
            pass

    # 3) og:image meta
    m3 = re.search(r'<meta\s+property=["\']og:image["\']\s+content=["\']([^"\']+)["\']', html, re.I)
    if m3 and not result['thumbnail']:
        t = m3.group(1).split('?')[0]
        if 'i.ytimg.com' in t:
            result['thumbnail'] = t

    # 4) meta description
    m4 = re.search(r'<meta\s+name=["\']description["\']\s+content=["\']([^"\']+)["\']', html, re.I)
    if m4 and not result['description']:
        result['description'] = m4.group(1)

    # 5) generic i.ytimg search
    m5 = re.search(r'(https?://i\.ytimg\.com/[^\s"\'<>]+)', html)
    if m5 and not result['thumbnail']:
        result['thumbnail'] = m5.group(1).split('?')[0]

    # 6) fallback: search for uploadDate anywhere
    if not result['upload_date']:
        m6 = re.search(r'"uploadDate"\s*:\s*"(\d{4}-\d{2}-\d{2})"', html)
        if m6:
            result['upload_date'] = m6.group(1).replace('-', '')

    return result

def YT_DLP_search(query, order, searchLimit, page, strategy):
    socket.setdefaulttimeout(30)

    ydl_opts = {
        'format': 'best',
        'outtmpl': '%(title)s.%(ext)s',
        # for search results we want lightweight entries then fill missing fields from HTML
        'extract_flat': True,
        'quiet': True,
        'skip_download': True,
    }

    session = requests.Session()
    session.headers.update(HEADERS)

    try:
        with yt_dlp.YoutubeDL(ydl_opts) as ydl:
            search = ydl.extract_info(f'ytsearch{searchLimit}:{query}', download=False)
    except Exception:
        logging.warning('Search failed.\n' + traceback.format_exc())
        strategy.onSearchFinished([])
        return

    entities = []
    yt_videos = search.get('entries') or []
    ii = 0
    for yt_object in yt_videos:
        try:
            # basic fields from yt-dlp (may be lightweight because of extract_flat)
            title = yt_object.get("fulltitle") or yt_object.get("title")
            webpage_url = yt_object.get("webpage_url") or yt_object.get("url")
            published = yt_object.get("upload_date")  # yt-dlp format YYYYMMDD or None
            duration = yt_object.get("duration")
            description = yt_object.get("description")
            vid = yt_object.get("id")
            view_count = yt_object.get("view_count")
            thumbnail = yt_object.get("thumbnail")

            # If any of description/upload_date/thumbnail is missing, fetch HTML and try regex/json fallbacks
            if (not description or not published or not thumbnail) and webpage_url:
                html = fetch_html(webpage_url)
                if html:
                    parsed = regex_extract_from_html(html)
                    if not description and parsed.get('description'):
                        description = parsed['description']
                    if not published and parsed.get('upload_date'):
                        published = parsed['upload_date']
                    if not thumbnail and parsed.get('thumbnail'):
                        thumbnail = parsed['thumbnail']

            # final thumbnail fallback: construct i.ytimg from id if still missing
            if not thumbnail and vid:
                thumbnail = f"https://i.ytimg.com/vi/{vid}/hqdefault.jpg"

            entity = {
                "title": title,
                "url": webpage_url,
                "published": published,
                "duration": duration,
                "description": description,
                "id": vid,
                "viewCount": view_count,
                "thumbnailUrl": thumbnail,
            }
            entities.append(entity)

            ii += 1
            if ii == searchLimit:
                break

        except Exception:
            logging.warning('Item adding skipped.\n' + traceback.format_exc())

    strategy.onSearchFinished(entities)


def YT_DLP_extractDirectLinks(link, receiver) :
    socket.setdefaulttimeout(30)

    ydl_opts = {
        'format': 'best',
        'get-url': True,
        'noplaylist': True,
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
