# MFT Search

Fast, zero-dependency File Search tool for Windows.
It searches all the NTFS drives on your device instantly by querying the Master File Table (MFT) and the USN Journal directly.

## Compiling
```powershell
g++ mft_search.cpp app.res -o mft_search.exe -mwindows -lcomctl32 -lole32 -lshell32 -lshlwapi -luuid
```
You must run the executable as an **Administrator**! 

## Features

### View (Sorting)
Accessible via the **View ▲** button or shortcuts:
- **Sort by**: Name (`Ctrl+1`), Path (`Ctrl+2`), Size (`Ctrl+3`), Extension (`Ctrl+4`), Date Modified (`Ctrl+5`), Date Created (`Ctrl+6`), Date Accessed (`Ctrl+7`).
- Toggle between **Ascending** and **Descending** order.

### Search (Filters & Logic)
Accessible via the **Search ▲** button or shortcuts:
- **Match Options**: Match Case (`Ctrl+I`), Whole Word (`Ctrl+B`), Path (`Ctrl+U`), Diacritics (`Ctrl+M`).
- **Regular Expressions**: Enable Regex (`Ctrl+R`) for advanced pattern matching (e.g., `^test`, `\.txt$`).
- **Quick Filters**: Instantly narrow results to Audio, Compressed, Documents, Executables, Folders, Pictures, or Videos.

### Global Actions
- **Refresh**: Press `F5` or right-click empty space to re-index all NTFS drives.
- **Context Menu**: Right-click any file for standard Windows shell options (Open, Copy, etc.).

## Authorship & License
**Authors:** iorwhAt

This is an open source project. The core philosophy is that anyone who contributes, patches, or pushes updates natively becomes a co-author of the application! Feel free to fork, modify, submit pull requests, and improve the search engine.
