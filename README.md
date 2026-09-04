# HaikuDownloader

A simple, native GUI for downloading YouTube videos and audio on Haiku OS.

I wrote this because I wanted a way to use yt-dlp without having to jump into the Terminal every time. It’s written in C++ using the native BeAPI, so it’s fast, lightweight, and looks right at home on the desktop.
What it does:

    Drag and Drop: Just drag / copy & paste a link from WebPositive (or any browser) into the URL field.

    Video & Audio: Pick between different audio and video formats before downloading your file.

    Quality Control: Select your resolution (from 144p up to 1080p).

    Tracker Integration: It saves the source URL into the file's attributes. You can see where a file came from by adding a "URL" column in any Tracker window.

    Cleanups: If you hit "Stop," it kills the process immediately and wipes the unfinished .part files so they don't clutter your folder.

    System Notifications: It sends a standard Haiku notification when the download finishes.

Requirements:

You’ll need yt-dlp and ffmpeg installed for this to work:
code Bash

pkgman install yt_dlp ffmpeg

Building from source:

If you want to compile it yourself, open a Terminal in the project folder and run:
code Bash

g++ -o HaikuDownloader Main.cpp App.cpp MainWindow.cpp -lbe -ltracker -lroot -llocale

License:

MIT. Do what you please.
