# MFT Search

Fast, zero-dependency File Search tool for Windows.
It searches all the NTFS drives on your device instantly by querying the Master File Table (MFT) and the USN Journal directly.

## What this does:
* Scans indexed files into RAM in under 2 seconds.
* Filters all files instantly on every keystroke using pre-processed memory blocks.
* No background services running, idle CPU usage, and disk usage required.

## Compiling
```powershell
g++ mft_search.cpp app.res -o mft_search.exe -mwindows -lcomctl32 -lole32 -lshell32 -lshlwapi -luuid
```
You must run the executable as an **Administrator**! 

## Authorship & License
**Authors:** iorwhAt

This is an open source project. The core philosophy is that anyone who contributes, patches, or pushes updates natively becomes a co-author of the application! Feel free to fork, modify, submit pull requests, and improve the search engine.
