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


install_and_import(
    "pytubefix"
    #"https://github.com/JuanBindez/pytubefix/archive/refs/heads/main.zip"
)

from pytubefix import Search, YouTube

# generate_po_token_substitute

from pytubefix.botGuard.bot_guard import NODE_PATH, VM_PATH

print("NODE_PATH =", NODE_PATH)
print("VM_PATH   =", VM_PATH)

import logging
import traceback


def YouTube_search(
    query,
    order,
    searchLimit,
    page,
    strategy
):
    socket.setdefaulttimeout(30)

    yt_watch = "https://www.youtube.com/watch?v="

    try:
        search = Search(query) #, 'WEB')
        yt_videos = search.results
    except Exception:
        logging.warning(
            'Search failed.\n' +
            traceback.format_exc()
        )
        strategy.onSearchFinished([])
        return

    entities = []

    if yt_videos is None:
        strategy.onSearchFinished([])
        return

    num = len(yt_videos)

    i = 0
    ii = 0

    while i < num:

        yt_object = yt_videos[i]

        try:

            #
            # Skip channels, playlists and anything
            # that is not a video result.
            #
            video_id = getattr(
                yt_object,
                "video_id",
                None
            )

            if not video_id:
                i += 1

                if i == num:

                    i = 0

                    yt_videos = (
                        search.get_next_results()
                    )

                    if yt_videos is None:
                        break

                    num = len(yt_videos)

                continue

            url = yt_watch + video_id

            try:
                yt_object.bypass_age_gate()
            except Exception:
                pass

            thumbnail = getattr(
                yt_object,
                "thumbnail_url",
                None
            )

            if not thumbnail:
                thumbnail = (
                    f"https://i.ytimg.com/vi/"
                    f"{video_id}/hqdefault.jpg"
                )

            entity = {}

            entity["title"] = getattr(
                yt_object,
                "title",
                None
            )

            entity["url"] = url

            entity["published"] = str(
                getattr(
                    yt_object,
                    "publish_date",
                    None
                )
            )

            entity["duration"] = getattr(
                yt_object,
                "length",
                None
            )

            entity["description"] = getattr(
                yt_object,
                "description",
                None
            )

            entity["id"] = video_id

            entity["viewCount"] = getattr(
                yt_object,
                "views",
                0
            )

            entity["thumbnailUrl"] = thumbnail

            entities.append(entity)

            ii += 1

            if ii >= searchLimit:
                break

        except Exception:
            logging.warning(
                'Item adding skipped.\n' +
                traceback.format_exc()
            )

        i += 1

        if i == num:

            i = 0

            try:
                yt_videos = (
                    search.get_next_results()
                )
            except Exception:
                break

            if yt_videos is None:
                break

            num = len(yt_videos)

    strategy.onSearchFinished(entities)


def _parse_height(stream):
    try:
        if stream.resolution:
            return int(stream.resolution.rstrip("p"))
    except Exception:
        pass

    return 0


def _parse_abr(stream):
    try:
        if stream.abr:
            return int(
                ''.join(
                    ch for ch in stream.abr
                    if ch.isdigit()
                )
            )
    except Exception:
        pass

    return 0


def YouTube_extractDirectLinks(
    link,
    preferred_height,
    receiver
):
    socket.setdefaulttimeout(30)

    try:
        yt = YouTube(link) #, 'WEB')
    except Exception:
        logging.warning(
            'Video initialization failed.\n' +
            traceback.format_exc()
        )

        receiver.onlinksExtracted({}, 0)
        return

    streams = list(yt.streams)

    #
    # Find best audio-only stream.
    #
    best_audio = None
    best_audio_abr = -1

    for stream in streams:

        try:

            if not getattr(
                stream,
                "includes_audio_track",
                False
            ):
                continue

            if getattr(
                stream,
                "includes_video_track",
                False
            ):
                continue

            abr = _parse_abr(stream)

            if abr > best_audio_abr:
                best_audio_abr = abr
                best_audio = stream

        except Exception:
            logging.warning(
                'Audio stream skipped.\n' +
                traceback.format_exc()
            )

    entities = {}
    i = 0

    #
    # Enumerate all video streams.
    #
    for stream in streams:

        try:

            has_video = getattr(
                stream,
                "includes_video_track",
                False
            )

            has_audio = getattr(
                stream,
                "includes_audio_track",
                False
            )

            if not has_video:
                continue

            entity = {}

            #
            # Progressive stream.
            #
            if has_audio:

                entity["url"] = stream.url

            #
            # Adaptive stream.
            #
            elif best_audio is not None:

                entity["url"] = (
                    stream.url +
                    "|" +
                    best_audio.url
                )

            else:
                continue

            entity["resolution"] = (
                stream.resolution
                or "unknown"
            )

            entity["extension"] = (
                stream.subtype
                or "mp4"
            )

            entity["_height"] = (
                _parse_height(stream)
            )

            entities[i] = entity
            i += 1

        except Exception:
            logging.warning(
                'Item adding skipped.\n' +
                traceback.format_exc()
            )

    try:
        preferred_height = int(
            preferred_height or 0
        )
    except (TypeError, ValueError):
        preferred_height = 0

    #
    # Same selection algorithm as YT-DLP.py
    #
    best = 0

    if entities:

        candidates = list(
            entities.items()
        )

        if preferred_height > 0:

            exact = [
                (index, entity)
                for index, entity in candidates
                if entity["_height"]
                == preferred_height
            ]

            if exact:

                best = exact[0][0]

            else:

                below = [
                    (index, entity)
                    for index, entity in candidates
                    if 0 <
                    entity["_height"] <
                    preferred_height
                ]

                if below:

                    best = max(
                        below,
                        key=lambda item: (
                            item[1]["_height"],
                            item[0]
                        )
                    )[0]

                else:

                    above = [
                        (index, entity)
                        for index, entity in candidates
                        if entity["_height"] > 0
                    ]

                    if above:

                        best = min(
                            above,
                            key=lambda item: (
                                item[1]["_height"],
                                item[0]
                            )
                        )[0]

                    else:

                        best = candidates[0][0]

        else:

            best = max(
                candidates,
                key=lambda item: (
                    item[1]["_height"],
                    item[0]
                )
            )[0]

        for entity in entities.values():
            entity.pop("_height", None)

    receiver.onlinksExtracted(
        entities,
        best
    )
