#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <windowsx.h>
#include <winioctl.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <algorithm>
#include <commctrl.h>
#include <shellapi.h>
#include <regex>

#define IDM_SORT_NAME       40001
#define IDM_SORT_PATH       40002
#define IDM_SORT_SIZE       40003
#define IDM_SORT_EXTENSION  40004
#define IDM_SORT_DATEMOD    40005
#define IDM_SORT_DATECRE    40006
#define IDM_SORT_DATEACC    40007
#define IDM_SORT_ASC        40010
#define IDM_SORT_DESC       40011
#define IDM_REFRESH         40020

#define IDM_MATCH_CASE      40030
#define IDM_MATCH_WORD      40031
#define IDM_MATCH_PATH      40032
#define IDM_MATCH_DIACRITICS 40033
#define IDM_ENABLE_REGEX    40034

#define IDM_FILTER_ALL      40050
#define IDM_FILTER_AUDIO    40051
#define IDM_FILTER_COMPRESSED 40052
#define IDM_FILTER_DOCUMENT 40053
#define IDM_FILTER_EXECUTABLE 40054
#define IDM_FILTER_FOLDER   40055
#define IDM_FILTER_PICTURE  40056
#define IDM_FILTER_VIDEO    40057

struct FileEntry {
    DWORDLONG frn;
    DWORDLONG parentFrn;
    DWORD attr;
    std::wstring name;
    std::wstring nameLower;
    wchar_t drive;
};

HWND hEdit, hList, hLblStatus, hBtnView, hBtnSearch;
HFONT g_hFont = NULL;
std::vector<FileEntry> g_allFiles;
std::unordered_map<DWORDLONG, size_t> g_frnMaps[26];
HMENU g_hSortMenu = NULL;

// Search match options
bool g_matchCase = false;
bool g_matchWord = false;
bool g_matchPath = false;
bool g_matchDiacritics = false;
bool g_enableRegex = false;
int  g_activeFilter = IDM_FILTER_ALL;

bool MatchesExtensionFilter(const std::wstring& nameLower) {
    if (g_activeFilter == IDM_FILTER_ALL) return true;
    size_t dot = nameLower.rfind(L'.');
    if (dot == std::wstring::npos) return (g_activeFilter == IDM_FILTER_ALL);
    std::wstring ext = nameLower.substr(dot);
    switch (g_activeFilter) {
        case IDM_FILTER_AUDIO:
            return ext==L".mp3"||ext==L".wav"||ext==L".flac"||ext==L".aac"||ext==L".ogg"||ext==L".wma"||ext==L".m4a"||ext==L".opus";
        case IDM_FILTER_COMPRESSED:
            return ext==L".zip"||ext==L".rar"||ext==L".7z"||ext==L".tar"||ext==L".gz"||ext==L".bz2"||ext==L".xz"||ext==L".cab";
        case IDM_FILTER_DOCUMENT:
            return ext==L".doc"||ext==L".docx"||ext==L".pdf"||ext==L".txt"||ext==L".rtf"||ext==L".xls"||ext==L".xlsx"||ext==L".ppt"||ext==L".pptx"||ext==L".odt"||ext==L".csv";
        case IDM_FILTER_EXECUTABLE:
            return ext==L".exe"||ext==L".msi"||ext==L".bat"||ext==L".cmd"||ext==L".com"||ext==L".scr"||ext==L".ps1";
        case IDM_FILTER_PICTURE:
            return ext==L".jpg"||ext==L".jpeg"||ext==L".png"||ext==L".gif"||ext==L".bmp"||ext==L".tiff"||ext==L".svg"||ext==L".webp"||ext==L".ico";
        case IDM_FILTER_VIDEO:
            return ext==L".mp4"||ext==L".avi"||ext==L".mkv"||ext==L".mov"||ext==L".wmv"||ext==L".flv"||ext==L".webm"||ext==L".m4v";
        default: return true;
    }
}

std::wstring GetFullPath(size_t index) {
    const FileEntry& entry = g_allFiles[index];
    std::wstring path = entry.name;
    DWORDLONG currentFrn = entry.frn;
    DWORDLONG parentFrn = entry.parentFrn;
    wchar_t drive = entry.drive;
    int driveIdx = towupper(drive) - L'A';
    
    int depth = 0;
    while (parentFrn != 0 && parentFrn != currentFrn && depth < 100) {
        auto it = g_frnMaps[driveIdx].find(parentFrn);
        if (it != g_frnMaps[driveIdx].end()) {
            size_t pIdx = it->second;
            std::wstring parentName = g_allFiles[pIdx].name;
            if (parentName != L"." && parentName != L"") {
                path = parentName + L"\\" + path;
            }
            currentFrn = parentFrn;
            parentFrn = g_allFiles[pIdx].parentFrn;
            depth++;
        } else {
            break;
        }
    }
    std::wstring root = L"X:\\";
    root[0] = drive;
    return root + path;
}

std::wstring FormatSize(DWORDLONG size) {
    if (size < 1024) return std::to_wstring(size) + L" B";
    if (size < 1024 * 1024) return std::to_wstring(size / 1024) + L" KB";
    if (size < 1024 * 1024 * 1024) return std::to_wstring(size / (1024 * 1024)) + L" MB";
    return std::to_wstring(size / (1024 * 1024 * 1024)) + L" GB";
}

std::wstring FormatTime(FILETIME ft) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
    wchar_t buf[256];
    swprintf(buf, 256, L"%04d-%02d-%02d %02d:%02d", stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute);
    return std::wstring(buf);
}

std::wstring GetExtension(const std::wstring& name) {
    size_t dot = name.rfind(L'.');
    if (dot != std::wstring::npos) {
        std::wstring ext = name.substr(dot);
        for (auto& c : ext) c = towlower(c);
        return ext;
    }
    return L"";
}

// sorting globals
int g_sortColumn = -1;
bool g_sortAscending = true;

int CALLBACK CompareListViewItems(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    if (lParam1 == -1) return 1; 
    if (lParam2 == -1) return -1;

    size_t idx1 = (size_t)lParam1;
    size_t idx2 = (size_t)lParam2;

    int cmp = 0;
    if (g_sortColumn == 0) { 
        cmp = wcsicmp(g_allFiles[idx1].name.c_str(), g_allFiles[idx2].name.c_str());
    } else if (g_sortColumn == 1) { 
        cmp = wcsicmp(GetFullPath(idx1).c_str(), GetFullPath(idx2).c_str());
    } else if (g_sortColumn == 2) { 
        WIN32_FILE_ATTRIBUTE_DATA fad1, fad2;
        DWORDLONG s1 = 0, s2 = 0;
        if (GetFileAttributesExW(GetFullPath(idx1).c_str(), GetFileExInfoStandard, &fad1) && !(fad1.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            s1 = ((DWORDLONG)fad1.nFileSizeHigh << 32) | fad1.nFileSizeLow;
        if (GetFileAttributesExW(GetFullPath(idx2).c_str(), GetFileExInfoStandard, &fad2) && !(fad2.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            s2 = ((DWORDLONG)fad2.nFileSizeHigh << 32) | fad2.nFileSizeLow;
        
        if (s1 < s2) cmp = -1;
        else if (s1 > s2) cmp = 1;
    } else if (g_sortColumn == 3) { 
        WIN32_FILE_ATTRIBUTE_DATA fad1, fad2;
        FILETIME ft1 = {0}, ft2 = {0};
        if (GetFileAttributesExW(GetFullPath(idx1).c_str(), GetFileExInfoStandard, &fad1)) ft1 = fad1.ftLastWriteTime;
        if (GetFileAttributesExW(GetFullPath(idx2).c_str(), GetFileExInfoStandard, &fad2)) ft2 = fad2.ftLastWriteTime;

        ULARGE_INTEGER u1, u2;
        u1.LowPart = ft1.dwLowDateTime; u1.HighPart = ft1.dwHighDateTime;
        u2.LowPart = ft2.dwLowDateTime; u2.HighPart = ft2.dwHighDateTime;
        
        if (u1.QuadPart < u2.QuadPart) cmp = -1;
        else if (u1.QuadPart > u2.QuadPart) cmp = 1;
    } else if (g_sortColumn == 4) {
        cmp = wcsicmp(GetExtension(g_allFiles[idx1].name).c_str(), GetExtension(g_allFiles[idx2].name).c_str());
    } else if (g_sortColumn == 5) {
        WIN32_FILE_ATTRIBUTE_DATA fad1, fad2;
        FILETIME ft1 = {0}, ft2 = {0};
        if (GetFileAttributesExW(GetFullPath(idx1).c_str(), GetFileExInfoStandard, &fad1)) ft1 = fad1.ftCreationTime;
        if (GetFileAttributesExW(GetFullPath(idx2).c_str(), GetFileExInfoStandard, &fad2)) ft2 = fad2.ftCreationTime;
        ULARGE_INTEGER uc1, uc2;
        uc1.LowPart = ft1.dwLowDateTime; uc1.HighPart = ft1.dwHighDateTime;
        uc2.LowPart = ft2.dwLowDateTime; uc2.HighPart = ft2.dwHighDateTime;
        if (uc1.QuadPart < uc2.QuadPart) cmp = -1;
        else if (uc1.QuadPart > uc2.QuadPart) cmp = 1;
    } else if (g_sortColumn == 6) {
        WIN32_FILE_ATTRIBUTE_DATA fad1, fad2;
        FILETIME ft1 = {0}, ft2 = {0};
        if (GetFileAttributesExW(GetFullPath(idx1).c_str(), GetFileExInfoStandard, &fad1)) ft1 = fad1.ftLastAccessTime;
        if (GetFileAttributesExW(GetFullPath(idx2).c_str(), GetFileExInfoStandard, &fad2)) ft2 = fad2.ftLastAccessTime;
        ULARGE_INTEGER ua1, ua2;
        ua1.LowPart = ft1.dwLowDateTime; ua1.HighPart = ft1.dwHighDateTime;
        ua2.LowPart = ft2.dwLowDateTime; ua2.HighPart = ft2.dwHighDateTime;
        if (ua1.QuadPart < ua2.QuadPart) cmp = -1;
        else if (ua1.QuadPart > ua2.QuadPart) cmp = 1;
    }

    return g_sortAscending ? cmp : -cmp;
}

void UpdateSortArrow(HWND hList) {
    HWND hHeader = ListView_GetHeader(hList);
    int colCount = Header_GetItemCount(hHeader);
    for (int i = 0; i < colCount; i++) {
        HDITEMW hdi = {0};
        hdi.mask = HDI_FORMAT;
        Header_GetItem(hHeader, i, &hdi);
        hdi.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == g_sortColumn) {
            hdi.fmt |= (g_sortAscending ? HDF_SORTUP : HDF_SORTDOWN);
        }
        Header_SetItem(hHeader, i, &hdi);
    }
}

void UpdateSortMenuChecks() {
    if (!g_hSortMenu) return;
    int sortIds[] = { IDM_SORT_NAME, IDM_SORT_PATH, IDM_SORT_SIZE, IDM_SORT_EXTENSION, IDM_SORT_DATEMOD, IDM_SORT_DATECRE, IDM_SORT_DATEACC };
    // Map g_sortColumn: 0=Name,1=Path,2=Size,3=DateMod,4=Ext,5=DateCre,6=DateAcc
    // Menu order: Name(0),Path(1),Size(2),Ext(3->4),DateMod(4->3),DateCre(5),DateAcc(6)
    for (int i = 0; i < 7; i++) CheckMenuItem(g_hSortMenu, sortIds[i], MF_UNCHECKED);
    int colToMenuIdx[] = { 0, 1, 2, 4, 3, 5, 6 };
    if (g_sortColumn >= 0 && g_sortColumn <= 6) CheckMenuItem(g_hSortMenu, sortIds[colToMenuIdx[g_sortColumn]], MF_CHECKED);
    CheckMenuItem(g_hSortMenu, IDM_SORT_ASC, g_sortAscending ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(g_hSortMenu, IDM_SORT_DESC, g_sortAscending ? MF_UNCHECKED : MF_CHECKED);
}

void ScanDrive(wchar_t driveLetter) {
    wchar_t volPath[] = L"\\\\.\\X:";
    volPath[4] = driveLetter;
    HANDLE hVol = CreateFileW(volPath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) return;

    USN_JOURNAL_DATA ujd;
    DWORD dwBytes;
    if (!DeviceIoControl(hVol, FSCTL_QUERY_USN_JOURNAL, NULL, 0, &ujd, sizeof(ujd), &dwBytes, NULL)) {
        CloseHandle(hVol);
        return;
    }

    MFT_ENUM_DATA med = {0};
    med.HighUsn = ujd.NextUsn;

    const int BUFFER_SIZE = 1024 * 1024;
    char* buffer = new char[BUFFER_SIZE];

    int driveIdx = towupper(driveLetter) - L'A';
    if (driveIdx < 0 || driveIdx >= 26) {
        delete[] buffer;
        CloseHandle(hVol);
        return;
    }

    while (true) {
        if (!DeviceIoControl(hVol, FSCTL_ENUM_USN_DATA, &med, sizeof(med), buffer, BUFFER_SIZE, &dwBytes, NULL)) {
            break;
        }

        med.StartFileReferenceNumber = *((DWORDLONG*)buffer);
        char* pRecord = buffer + sizeof(DWORDLONG);

        while (pRecord < buffer + dwBytes) {
            USN_RECORD* record = (USN_RECORD*)pRecord;
            std::wstring filename((wchar_t*)(pRecord + record->FileNameOffset), record->FileNameLength / 2);
            
            std::wstring lowerName = filename;
            for (auto& c : lowerName) c = towlower(c);

            size_t newIdx = g_allFiles.size();
            g_allFiles.push_back({record->FileReferenceNumber, record->ParentFileReferenceNumber, record->FileAttributes, filename, lowerName, driveLetter});
            g_frnMaps[driveIdx][record->FileReferenceNumber] = newIdx;

            pRecord += record->RecordLength;
        }
    }

    delete[] buffer;
    CloseHandle(hVol);
}

void BuildFileIndex() {
    auto start = std::chrono::high_resolution_clock::now();
    
    g_allFiles.clear();
    for (int i = 0; i < 26; i++) g_frnMaps[i].clear();

    g_allFiles.reserve(2000000);
    for (int i = 0; i < 26; i++) g_frnMaps[i].reserve(100000);

    wchar_t driveStrings[512];
    DWORD len = GetLogicalDriveStringsW(511, driveStrings);
    if (len == 0 || len > 511) {
        SetWindowTextW(hLblStatus, L"Failed to get logical drives.");
        return;
    }

    wchar_t* drive = driveStrings;
    bool anyScanned = false;
    while (*drive) {
        UINT type = GetDriveTypeW(drive);
        if (type == DRIVE_FIXED) {
            wchar_t fsName[MAX_PATH];
            if (GetVolumeInformationW(drive, NULL, 0, NULL, NULL, NULL, fsName, MAX_PATH)) {
                if (wcscmp(fsName, L"NTFS") == 0) {
                    ScanDrive(drive[0]);
                    anyScanned = true;
                }
            }
        }
        drive += wcslen(drive) + 1;
    }

    if (!anyScanned && g_allFiles.empty()) {
        SetWindowTextW(hLblStatus, L"No NTFS drives found or access denied. Run as administrator.");
        return;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    
    wchar_t statusMsg[256];
    swprintf(statusMsg, 256, L"indexed %zu items across all NTFS drives in %.2fs", g_allFiles.size(), diff.count());
    SetWindowTextW(hLblStatus, statusMsg);
}

void PerformSearch() {
    SendMessage(hList, WM_SETREDRAW, FALSE, 0);
    SendMessage(hList, LVM_DELETEALLITEMS, 0, 0);

    wchar_t queryBuf[256];
    GetWindowTextW(hEdit, queryBuf, 256);
    std::wstring query(queryBuf);

    if (query.empty()) {
        SendMessage(hList, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(hList, NULL, TRUE);
        SetWindowTextW(hLblStatus, L"ready");
        return;
    }

    for (auto& c : query) c = towlower(c);

    std::wstring queryOriginal(queryBuf);

    int foundCount = 0;
    int row = 0;

    for (size_t i = 0; i < g_allFiles.size(); i++) {
        // Filter by file type
        if (g_activeFilter == IDM_FILTER_FOLDER) {
            if (!(g_allFiles[i].attr & FILE_ATTRIBUTE_DIRECTORY)) continue;
        } else if (g_activeFilter != IDM_FILTER_ALL) {
            if (g_allFiles[i].attr & FILE_ATTRIBUTE_DIRECTORY) continue;
            if (!MatchesExtensionFilter(g_allFiles[i].nameLower)) continue;
        }

        // Determine search target and query based on options
        bool matched = false;
        if (g_enableRegex) {
            try {
                std::wstring target = g_matchPath ? GetFullPath(i) : g_allFiles[i].name;
                std::wregex re(queryOriginal, g_matchCase ? std::regex_constants::extended : (std::regex_constants::extended | std::regex_constants::icase));
                matched = std::regex_search(target, re);
            } catch (...) {
                // Invalid regex, maybe show an error once?
                matched = false;
            }
        } else if (g_matchCase) {
            const std::wstring& target = g_matchPath ? GetFullPath(i) : g_allFiles[i].name;
            if (g_matchWord) {
                matched = (target == queryOriginal);
            } else {
                matched = (target.find(queryOriginal) != std::wstring::npos);
            }
        } else {
            std::wstring target = g_matchPath ? GetFullPath(i) : g_allFiles[i].nameLower;
            if (g_matchPath) { for (auto& c : target) c = towlower(c); }
            if (g_matchWord) {
                matched = (target == query);
            } else {
                matched = (target.find(query) != std::wstring::npos);
            }
        }

        if (matched) {
            std::wstring fullPath = GetFullPath(i);
            
            SHFILEINFOW sfi = {0};
            SHGetFileInfoW(fullPath.c_str(), g_allFiles[i].attr, &sfi, sizeof(sfi), 
                           SHGFI_SYSICONINDEX | SHGFI_USEFILEATTRIBUTES | SHGFI_SMALLICON);

            LVITEMW lvi = {0};
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM;
            lvi.iItem = row;
            lvi.iImage = sfi.iIcon;
            lvi.pszText = (LPWSTR)g_allFiles[i].name.c_str();
            lvi.lParam = (LPARAM)i;
            SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

            LVITEMW lviSub = {0};
            lviSub.mask = LVIF_TEXT;
            lviSub.iItem = row;
            lviSub.iSubItem = 1;
            lviSub.pszText = (LPWSTR)fullPath.c_str();
            SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lviSub);

            WIN32_FILE_ATTRIBUTE_DATA fad;
            std::wstring sizeStr = L"";
            std::wstring timeStr = L"";
            if (GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fad)) {
                if (!(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    DWORDLONG sz = ((DWORDLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                    sizeStr = FormatSize(sz);
                }
                timeStr = FormatTime(fad.ftLastWriteTime);
            }

            lviSub.iSubItem = 2;
            lviSub.pszText = (LPWSTR)sizeStr.c_str();
            SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lviSub);

            lviSub.iSubItem = 3;
            lviSub.pszText = (LPWSTR)timeStr.c_str();
            SendMessageW(hList, LVM_SETITEMW, 0, (LPARAM)&lviSub);

            row++;
            foundCount++;
            
            if (foundCount >= 300) {
                LVITEMW lviLast = {0};
                lviLast.mask = LVIF_TEXT | LVIF_PARAM;
                lviLast.iItem = row;
                lviLast.pszText = (LPWSTR)L"...";
                lviLast.lParam = (LPARAM)-1;
                SendMessageW(hList, LVM_INSERTITEMW, 0, (LPARAM)&lviLast);
                break;
            }
        }
    }
    
    if (g_sortColumn != -1) {
        ListView_SortItems(hList, CompareListViewItems, 0);
        UpdateSortArrow(hList);
    }
    
    SendMessage(hList, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hList, NULL, TRUE);

    wchar_t statusMsg[256];
    swprintf(statusMsg, 256, L"results: %d", foundCount);
    SetWindowTextW(hLblStatus, statusMsg);
}

void ShowShellContextMenu(HWND hwnd, const std::wstring& path, POINT pt) {
    LPITEMIDLIST pidl;
    if (FAILED(SHParseDisplayName(path.c_str(), NULL, &pidl, 0, NULL))) return;

    IShellFolder* psfParent;
    LPCITEMIDLIST pidlChild;
    if (FAILED(SHBindToParent(pidl, IID_IShellFolder, (void**)&psfParent, &pidlChild))) {
        CoTaskMemFree(pidl);
        return;
    }

    IContextMenu* pcm;
    if (SUCCEEDED(psfParent->GetUIObjectOf(hwnd, 1, &pidlChild, IID_IContextMenu, NULL, (void**)&pcm))) {
        HMENU hMenu = CreatePopupMenu();
        if (SUCCEEDED(pcm->QueryContextMenu(hMenu, 0, 1, 0x7FFF, CMF_NORMAL))) {
            int cmd = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);
            if (cmd > 0) {
                CMINVOKECOMMANDINFO ici = { sizeof(ici) };
                ici.lpVerb = MAKEINTRESOURCEA(cmd - 1);
                ici.nShow = SW_SHOWNORMAL;
                ici.hwnd = hwnd;
                pcm->InvokeCommand(&ici);
            }
        }
        pcm->Release();
        DestroyMenu(hMenu);
    }
    psfParent->Release();
    CoTaskMemFree(pidl);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            INITCOMMONCONTROLSEX icex;
            icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
            icex.dwICC  = ICC_LISTVIEW_CLASSES;
            InitCommonControlsEx(&icex);
            
            hLblStatus = CreateWindowW(L"STATIC", L"MFT Search needs to be run as administrator for it to work.",
                                       WS_CHILD | WS_VISIBLE, 10, 10, 960, 20, hwnd, NULL, NULL, NULL);

            hEdit = CreateWindowW(L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 10, 40, 960, 25, hwnd, NULL, NULL, NULL);

            hList = CreateWindowW(WC_LISTVIEWW, L"",
                                  WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                                  10, 80, 960, 460, hwnd, NULL, GetModuleHandle(NULL), NULL);

            ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

            SHFILEINFOW sfi;
            HIMAGELIST himl = (HIMAGELIST)SHGetFileInfoW(L"C:\\", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
            SendMessageW(hList, LVM_SETIMAGELIST, LVSIL_SMALL, (LPARAM)himl);

            g_hFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            LVCOLUMNW lvc = {0};
            lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
            lvc.pszText = (LPWSTR)L"Name";
            lvc.cx = 300;
            SendMessageW(hList, LVM_INSERTCOLUMNW, 0, (LPARAM)&lvc);

            lvc.pszText = (LPWSTR)L"Path";
            lvc.cx = 440;
            SendMessageW(hList, LVM_INSERTCOLUMNW, 1, (LPARAM)&lvc);

            lvc.pszText = (LPWSTR)L"Size";
            lvc.cx = 80;
            SendMessageW(hList, LVM_INSERTCOLUMNW, 2, (LPARAM)&lvc);

            lvc.pszText = (LPWSTR)L"Date Modified";
            lvc.cx = 120;
            SendMessageW(hList, LVM_INSERTCOLUMNW, 3, (LPARAM)&lvc);

            HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            SendMessage(hLblStatus, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            SendMessage(hList, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            SendMessage(hEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELONG(10, 10));

            hBtnView = CreateWindowW(L"BUTTON", L"View \x25B2",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     10, 0, 60, 22, hwnd, (HMENU)1001, NULL, NULL);
            SendMessage(hBtnView, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            hBtnSearch = CreateWindowW(L"BUTTON", L"Search \x25B2",
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                       75, 0, 75, 22, hwnd, (HMENU)1002, NULL, NULL);
            SendMessage(hBtnSearch, WM_SETFONT, (WPARAM)g_hFont, TRUE);
            break;
        }
        case WM_SIZE: {
            if (hEdit && hList && hLblStatus) {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                MoveWindow(hEdit, 10, 12, width - 20, 24, TRUE);
                MoveWindow(hList, 10, 42, width - 20, height - 72, TRUE);
                // View button at bottom-left, Search button next to it, status label to the right
                MoveWindow(hBtnView, 10, height - 27, 60, 22, TRUE);
                MoveWindow(hBtnSearch, 75, height - 27, 75, 22, TRUE);
                MoveWindow(hLblStatus, 158, height - 25, width - 168, 20, TRUE);
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR nmh = (LPNMHDR)lParam;
            if (nmh->hwndFrom == hList) {
                if (nmh->code == NM_DBLCLK) {
                    LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
                    if (lpnmitem->iItem != -1) {
                        LVITEMW lvi = {0};
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = lpnmitem->iItem;
                        SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi);
                        
                        size_t idx = (size_t)lvi.lParam;
                        if (idx != (size_t)-1) {
                            std::wstring path = GetFullPath(idx);
                            ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                        }
                    }
                } else if (nmh->code == LVN_COLUMNCLICK) {
                    LPNMLISTVIEW pnmv = (LPNMLISTVIEW)lParam;
                    if (g_sortColumn == pnmv->iSubItem) {
                        g_sortAscending = !g_sortAscending;
                    } else {
                        g_sortColumn = pnmv->iSubItem;
                        g_sortAscending = true;
                    }
                    ListView_SortItems(hList, CompareListViewItems, 0);
                    UpdateSortArrow(hList);
                    UpdateSortMenuChecks();
                }
            }
            break;
        }
        case WM_COMMAND: {
            if ((HWND)lParam == hEdit) {
                if (HIWORD(wParam) == EN_CHANGE) {
                    PerformSearch();
                }
            } else if (LOWORD(wParam) == 1001 && (HWND)lParam == hBtnView) {
                // View button clicked - show sort popup above the button
                RECT rc;
                GetWindowRect(hBtnView, &rc);
                UpdateSortMenuChecks();
                TrackPopupMenu(g_hSortMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, rc.left, rc.top, 0, hwnd, NULL);
            } else if (LOWORD(wParam) == 1002 && (HWND)lParam == hBtnSearch) {
                // Search button clicked - show match/filter popup above the button
                HMENU hSearchMenu = CreatePopupMenu();
                AppendMenuW(hSearchMenu, MF_STRING | (g_matchCase ? MF_CHECKED : 0), IDM_MATCH_CASE, L"Match Case\tCtrl+I");
                AppendMenuW(hSearchMenu, MF_STRING | (g_matchWord ? MF_CHECKED : 0), IDM_MATCH_WORD, L"Match Whole Word\tCtrl+B");
                AppendMenuW(hSearchMenu, MF_STRING | (g_matchPath ? MF_CHECKED : 0), IDM_MATCH_PATH, L"Match Path\tCtrl+U");
                AppendMenuW(hSearchMenu, MF_STRING | (g_matchDiacritics ? MF_CHECKED : 0), IDM_MATCH_DIACRITICS, L"Match Diacritics\tCtrl+M");
                AppendMenuW(hSearchMenu, MF_SEPARATOR, 0, NULL);
                AppendMenuW(hSearchMenu, MF_STRING | (g_enableRegex ? MF_CHECKED : 0), IDM_ENABLE_REGEX, L"Enable Regex\tCtrl+R");
                AppendMenuW(hSearchMenu, MF_SEPARATOR, 0, NULL);
                int filters[] = { IDM_FILTER_ALL, IDM_FILTER_AUDIO, IDM_FILTER_COMPRESSED, IDM_FILTER_DOCUMENT, IDM_FILTER_EXECUTABLE, IDM_FILTER_FOLDER, IDM_FILTER_PICTURE, IDM_FILTER_VIDEO };
                const wchar_t* filterNames[] = { L"Everything", L"Audio", L"Compressed", L"Document", L"Executable", L"Folder", L"Picture", L"Video" };
                for (int fi = 0; fi < 8; fi++) {
                    AppendMenuW(hSearchMenu, MF_STRING | (g_activeFilter == filters[fi] ? MF_CHECKED : 0), filters[fi], filterNames[fi]);
                }
                RECT rc;
                GetWindowRect(hBtnSearch, &rc);
                TrackPopupMenu(hSearchMenu, TPM_LEFTALIGN | TPM_BOTTOMALIGN, rc.left, rc.top, 0, hwnd, NULL);
                DestroyMenu(hSearchMenu);
            } else {
                int menuId = LOWORD(wParam);
                int newSortCol = -1;
                switch (menuId) {
                    case IDM_SORT_NAME:      newSortCol = 0; break;
                    case IDM_SORT_PATH:      newSortCol = 1; break;
                    case IDM_SORT_SIZE:      newSortCol = 2; break;
                    case IDM_SORT_EXTENSION: newSortCol = 4; break;
                    case IDM_SORT_DATEMOD:   newSortCol = 3; break;
                    case IDM_SORT_DATECRE:   newSortCol = 5; break;
                    case IDM_SORT_DATEACC:   newSortCol = 6; break;
                    case IDM_SORT_ASC:
                        g_sortAscending = true;
                        if (g_sortColumn >= 0) { ListView_SortItems(hList, CompareListViewItems, 0); UpdateSortArrow(hList); }
                        UpdateSortMenuChecks();
                        break;
                    case IDM_SORT_DESC:
                        g_sortAscending = false;
                        if (g_sortColumn >= 0) { ListView_SortItems(hList, CompareListViewItems, 0); UpdateSortArrow(hList); }
                        UpdateSortMenuChecks();
                        break;
                    case IDM_REFRESH:
                        SetWindowTextW(hLblStatus, L"Refreshing index...");
                        BuildFileIndex();
                        PerformSearch();
                        break;
                    case IDM_MATCH_CASE:   g_matchCase = !g_matchCase; PerformSearch(); break;
                    case IDM_MATCH_WORD:   g_matchWord = !g_matchWord; PerformSearch(); break;
                    case IDM_MATCH_PATH:   g_matchPath = !g_matchPath; PerformSearch(); break;
                    case IDM_MATCH_DIACRITICS: g_matchDiacritics = !g_matchDiacritics; PerformSearch(); break;
                    case IDM_ENABLE_REGEX: g_enableRegex = !g_enableRegex; PerformSearch(); break;
                    case IDM_FILTER_ALL: case IDM_FILTER_AUDIO: case IDM_FILTER_COMPRESSED:
                    case IDM_FILTER_DOCUMENT: case IDM_FILTER_EXECUTABLE: case IDM_FILTER_FOLDER:
                    case IDM_FILTER_PICTURE: case IDM_FILTER_VIDEO:
                        g_activeFilter = LOWORD(wParam);
                        PerformSearch();
                        break;
                }
                if (newSortCol >= 0) {
                    if (g_sortColumn == newSortCol) g_sortAscending = !g_sortAscending;
                    else { g_sortColumn = newSortCol; g_sortAscending = true; }
                    ListView_SortItems(hList, CompareListViewItems, 0);
                    UpdateSortArrow(hList);
                    UpdateSortMenuChecks();
                }
            }
            break;
        }
        case WM_CONTEXTMENU: {
            if ((HWND)wParam == hList) {
                POINT pt;
                if (lParam == -1) {
                    pt.x = 0; pt.y = 0;
                    ClientToScreen(hList, &pt);
                } else {
                    pt.x = GET_X_LPARAM(lParam);
                    pt.y = GET_Y_LPARAM(lParam);
                }

                // Hit-test to see if right-click is on an item
                LVHITTESTINFO hti = {0};
                hti.pt.x = pt.x; hti.pt.y = pt.y;
                ScreenToClient(hList, &hti.pt);
                int hitItem = (int)SendMessageW(hList, LVM_HITTEST, 0, (LPARAM)&hti);

                if (hitItem != -1) {
                    // Clicked on an item - show shell context menu
                    LVITEMW lvi = {0};
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = hitItem;
                    SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi);
                    size_t idx = (size_t)lvi.lParam;
                    if (idx != (size_t)-1) {
                        ShowShellContextMenu(hwnd, GetFullPath(idx), pt);
                    }
                } else {
                    // Clicked on empty space - show Refresh + Sort by
                    HMENU hCtxMenu = CreatePopupMenu();
                    AppendMenuW(hCtxMenu, MF_STRING, IDM_REFRESH, L"Refresh\tF5");
                    AppendMenuW(hCtxMenu, MF_SEPARATOR, 0, NULL);
                    UpdateSortMenuChecks();
                    AppendMenuW(hCtxMenu, MF_POPUP, (UINT_PTR)g_hSortMenu, L"Sort by");
                    TrackPopupMenu(hCtxMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                    // Remove the sort submenu reference before destroying so g_hSortMenu survives
                    for (int ri = GetMenuItemCount(hCtxMenu) - 1; ri >= 0; ri--) {
                        MENUITEMINFOW mii = { sizeof(mii) };
                        mii.fMask = MIIM_SUBMENU;
                        GetMenuItemInfoW(hCtxMenu, ri, TRUE, &mii);
                        if (mii.hSubMenu == g_hSortMenu) { RemoveMenu(hCtxMenu, ri, MF_BYPOSITION); break; }
                    }
                    DestroyMenu(hCtxMenu);
                }
            }
            break;
        }
        case WM_DESTROY: {
            if (g_hFont) DeleteObject(g_hFont);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    CoInitialize(NULL);
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MftGUIClass";
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

    RegisterClassW(&wc);

    g_hSortMenu = CreatePopupMenu();
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_NAME,      L"Name\tCtrl+1");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_PATH,      L"Path\tCtrl+2");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_SIZE,      L"Size\tCtrl+3");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_EXTENSION, L"Extension\tCtrl+4");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_DATEMOD,   L"Date Modified\tCtrl+5");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_DATECRE,   L"Date Created\tCtrl+6");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_DATEACC,   L"Date Accessed\tCtrl+7");
    AppendMenuW(g_hSortMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_ASC,       L"Ascending");
    AppendMenuW(g_hSortMenu, MF_STRING, IDM_SORT_DESC,      L"Descending");

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"MFT Search",
                              WS_OVERLAPPEDWINDOW, 
                              CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
                              NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    BuildFileIndex();

    ACCEL accels[] = {
        { FCONTROL | FVIRTKEY, '1', IDM_SORT_NAME },
        { FCONTROL | FVIRTKEY, '2', IDM_SORT_PATH },
        { FCONTROL | FVIRTKEY, '3', IDM_SORT_SIZE },
        { FCONTROL | FVIRTKEY, '4', IDM_SORT_EXTENSION },
        { FCONTROL | FVIRTKEY, '5', IDM_SORT_DATEMOD },
        { FCONTROL | FVIRTKEY, '6', IDM_SORT_DATECRE },
        { FCONTROL | FVIRTKEY, '7', IDM_SORT_DATEACC },
        { FVIRTKEY, VK_F5, IDM_REFRESH },
        { FCONTROL | FVIRTKEY, 'I', IDM_MATCH_CASE },
        { FCONTROL | FVIRTKEY, 'B', IDM_MATCH_WORD },
        { FCONTROL | FVIRTKEY, 'U', IDM_MATCH_PATH },
        { FCONTROL | FVIRTKEY, 'M', IDM_MATCH_DIACRITICS },
        { FCONTROL | FVIRTKEY, 'R', IDM_ENABLE_REGEX },
    };
    HACCEL hAccel = CreateAcceleratorTableW(accels, 13);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}
