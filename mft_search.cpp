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

struct FileEntry {
    DWORDLONG frn;
    DWORDLONG parentFrn;
    DWORD attr;
    std::wstring name;
    std::wstring nameLower;
    wchar_t drive;
};

HWND hEdit, hList, hLblStatus;
HFONT g_hFont = NULL;
std::vector<FileEntry> g_allFiles;
std::unordered_map<DWORDLONG, size_t> g_frnMaps[26];

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

    int foundCount = 0;
    int row = 0;

    for (size_t i = 0; i < g_allFiles.size(); i++) {
        if (g_allFiles[i].nameLower.find(query) != std::wstring::npos) {
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
            break;
        }
        case WM_SIZE: {
            if (hEdit && hList && hLblStatus) {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                // Search at the top with a bit more margin
                MoveWindow(hEdit, 10, 12, width - 20, 24, TRUE);
                // List in the middle
                MoveWindow(hList, 10, 42, width - 20, height - 72, TRUE);
                // Status at the bottom
                MoveWindow(hLblStatus, 10, height - 25, width - 20, 20, TRUE);
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
                }
            }
            break;
        }
        case WM_COMMAND: {
            if ((HWND)lParam == hEdit) {
                if (HIWORD(wParam) == EN_CHANGE) {
                    PerformSearch();
                }
            }
            break;
        }
        case WM_CONTEXTMENU: {
            if ((HWND)wParam == hList) {
                int selRow = SendMessageW(hList, LVM_GETNEXTITEM, -1, LVNI_SELECTED);
                if (selRow != -1) {
                    LVITEMW lvi = {0};
                    lvi.mask = LVIF_PARAM;
                    lvi.iItem = selRow;
                    SendMessageW(hList, LVM_GETITEMW, 0, (LPARAM)&lvi);
                    
                    size_t idx = (size_t)lvi.lParam;
                    if (idx != (size_t)-1) {
                        POINT pt;
                        if (lParam == -1) {
                            RECT rc;
                            ListView_GetItemRect(hList, selRow, &rc, LVIR_BOUNDS);
                            pt.x = rc.left + 5;
                            pt.y = rc.top + 5;
                            ClientToScreen(hList, &pt);
                        } else {
                            pt.x = LOWORD(lParam);
                            pt.y = HIWORD(lParam);
                        }
                        
                        ShowShellContextMenu(hwnd, GetFullPath(idx), pt);
                    }
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

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"MFT Search",
                              WS_OVERLAPPEDWINDOW, 
                              CW_USEDEFAULT, CW_USEDEFAULT, 1000, 600,
                              NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    BuildFileIndex();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
