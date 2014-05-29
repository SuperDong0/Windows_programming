#define _CRT_SECURE_NO_WARNINGS

#define NDEBUG

#include "resource.h"
#include "helper.h"
#include <initguid.h>
#include "shell_ext_iid.h"
#include <cstdio>
#include <clocale>
#include <vector>
#include <shlobj.h>
#include <ShlGuid.h>
#include <Shlwapi.h>
#include <tchar.h>

using std::vector;

namespace {

struct FilePath {
        TCHAR data[MAX_PATH];
};

HINSTANCE g_this_dll        = NULL;
ULONG g_lock_count          = 0;
ULONG g_obj_count           = 0;
ULONG g_page_count          = 0;
FILE* g_file                = NULL;

vector<FilePath> g_prop_sheet_file_list;
vector<FilePath> g_menu_file_list;


//-------------------------------------------------
//                  COM classes
//-------------------------------------------------
class PropPage : public IShellExtInit, public IShellPropSheetExt {
public:
        PropPage();
        virtual ~PropPage();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();
        
        STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, 
                                IDataObject *pdtobj, 
                                HKEY hkeyProgID);

        STDMETHODIMP AddPages(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam);

        STDMETHODIMP ReplacePage(UINT uPageID, 
                                 LPFNADDPROPSHEETPAGE pfnReplacePage, 
                                 LPARAM lParam);
private:
        ULONG ref_count_;
};

class ExeMenu : public IShellExtInit, public IContextMenu {
public:
        ExeMenu();
        virtual ~ExeMenu();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP Initialize(PCIDLIST_ABSOLUTE pidlFolder, 
                                IDataObject *pdtobj, 
                                HKEY hkeyProgID);

        STDMETHODIMP GetCommandString(UINT_PTR idCmd, 
                                      UINT uFlags,
                                      UINT* pwReserved,
                                      LPSTR pszName,
                                      UINT cchMax);

        STDMETHODIMP InvokeCommand(CMINVOKECOMMANDINFO* pici);

        STDMETHODIMP QueryContextMenu(HMENU hmenu, 
                                      UINT indexMenu,
                                      UINT idCmdFirst,
                                      UINT idCmdLast,
                                      UINT uFlags);
private:
        ULONG ref_count_;
};

class CopyMonitor : public ICopyHook {
public:
        CopyMonitor();
        virtual ~CopyMonitor();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP_(UINT) CopyCallback(HWND hwnd,
                                         UINT wFunc,
                                         UINT wFlags,
                                         LPCTSTR pszSrcFile,
                                         DWORD dwSrcAttribs,
                                         LPCTSTR pszDestFile,
                                         DWORD dwDestAttribs);
private:
        ULONG ref_count_;
};

class DropTxt : public IDropTarget, public IPersistFile {
public:
        DropTxt();
        virtual ~DropTxt();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP DragEnter(IDataObject *pDataObj,
                               DWORD grfKeyState,
                               POINTL pt,
                               DWORD *pdwEffect);

        STDMETHODIMP DragLeave();
        STDMETHODIMP DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect);

        STDMETHODIMP Drop(IDataObject *pDataObj,
                          DWORD grfKeyState,
                          POINTL pt,
                          DWORD *pdwEffect);
        
        STDMETHODIMP GetClassID(CLSID *pClassID);
        STDMETHODIMP GetCurFile(LPOLESTR *ppszFileName);
        STDMETHODIMP IsDirty();
        STDMETHODIMP Load(LPCOLESTR pszFileName, DWORD dwMode);
        STDMETHODIMP Save(LPCOLESTR pszFileName, BOOL fRemember);
        STDMETHODIMP SaveCompleted(LPCOLESTR pszFileName);
private:
        ULONG ref_count_;
        WCHAR target_path_[MAX_PATH];
};


//-------------------------------------------------
//               COM class factory
//-------------------------------------------------
class PropPageFactory : public IClassFactory {
public:
        PropPageFactory();
        virtual ~PropPageFactory();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);

private:
        ULONG ref_count_;
};

class ExeMenuFactory : public IClassFactory {
public:
        ExeMenuFactory();
        virtual ~ExeMenuFactory();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);

private:
        ULONG ref_count_;
};

class CopyMonitorFactory : public IClassFactory {
public:
        CopyMonitorFactory();
        virtual ~CopyMonitorFactory();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);

private:
        ULONG ref_count_;
};

class DropTxtFactory : public IClassFactory {
public:
        DropTxtFactory();
        virtual ~DropTxtFactory();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);
private:
        ULONG ref_count_;
};


//-------------------------------------------------
//                 Useful helpers
//-------------------------------------------------
#ifdef NDEBUG
void InitLogFile()
{
        g_file = fopen("C:\\Documents and Settings\\lausai\\My Documents\\"
                       "Dropbox\\my_code\\windows_shell\\log.txt", "w");
        fputs("shell_ext DLL_PROCESS_ATTACH\n", g_file);
        fflush(g_file);
}

void PrintIID(const TCHAR* prefix, REFIID riid)
{
        LPOLESTR iid   = NULL;
        HRESULT result = StringFromIID(riid, &iid);

        if (S_OK == result) {
                fwprintf(g_file, L"%ls iid=%ls\n", prefix, iid);
                fflush(g_file);
                CoTaskMemFree(iid);
        }
}

#define LOG(a, ...) \
        do {\
                _ftprintf(g_file, _T(a), __VA_ARGS__); \
                fflush(g_file); \
        } while (stdout == NULL)        // Use a trick to disable warning C4127

#else

#define InitLogFile()
#define PrintIID(a, b)
#define LOG(...)

#endif

HRESULT DoInitialize(IDataObject* pdtobj, vector<FilePath>* file_list)
{
        if (NULL == pdtobj) {
                LOG("Initialize E_INVALIDARG\n");
                return E_INVALIDARG;
        }
        
        // Property sheets are common controls and needed to initialize
        INITCOMMONCONTROLSEX ctrl = {sizeof(ctrl), ICC_WIN95_CLASSES};
        InitCommonControlsEx(&ctrl);
        
        // Get the name of the selected file
        STGMEDIUM medium = {};
        FORMATETC fe     = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        HRESULT result   = pdtobj->GetData(&fe, &medium);

        if (FAILED(result)) {
                LOG("Initialize E_INVALIDARG\n");
                return E_INVALIDARG;
        }

        HDROP drop     = static_cast<HDROP>(medium.hGlobal);
        UINT num_files = DragQueryFile(drop, 0xFFFFFFFF, NULL, 0);
        
        LOG("number of files=%u\n", num_files);
        file_list->clear();
        for (UINT i = 0; i < num_files; i++) {
                FilePath path = {};

                if (0 != DragQueryFile(drop, i, path.data, _countof(path.data))) {
                        file_list->push_back(path);
                        LOG("path=%ls\n", path.data);
                } else {
                        LOG("error code=%u\n", GetLastError());
                }
        }

        ReleaseStgMedium(&medium);
        LOG("Initialize S_OK\n");
        return S_OK;
}

INT_PTR CALLBACK PropDialogProc(HWND dialog, UINT uMsg, WPARAM, LPARAM lParam)
{
        TCHAR* file_path          = NULL;
        PROPSHEETPAGE* sheet_page = NULL;

        switch (uMsg) {
        case WM_INITDIALOG:
                sheet_page = reinterpret_cast<PROPSHEETPAGE*>(lParam);
                file_path  = reinterpret_cast<TCHAR*>(sheet_page->lParam);
                
                LOG("handle init dialog\n");

                SetDlgItemText(dialog, IDC_EDIT1, file_path);
                break;
        default:
                ;
        }

        return FALSE;
}

}       // End of unnamed namespace


//-------------------------------------------------
//                DLL entry point
//-------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
        static char* old_locale = NULL;

        switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
                g_this_dll = hinstDLL;
                old_locale = setlocale(LC_CTYPE, "");
                InitLogFile();
                break;
        case DLL_PROCESS_DETACH:
                LOG("shell_ext DLL_PROCESS_DETACH\n");
                if (g_file)
                        fclose(g_file);
                
                setlocale(LC_CTYPE, old_locale);
                break;
        default:
                ;
        }

        return TRUE;
}


//-------------------------------------------------
//                COM entry point
//-------------------------------------------------
// STDAPI means extern "C" HRESULT __stdcall
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
        IClassFactory* factory = NULL;

        if (CLSID_PropPage == rclsid)
                factory = new PropPageFactory();

        if (CLSID_ExeMenu == rclsid)
                factory = new ExeMenuFactory();

        if (CLSID_CopyMonitor == rclsid)
                factory = new CopyMonitorFactory();

        if (CLSID_DropTxt == rclsid)
                factory = new DropTxtFactory();

        if (factory) {
                HRESULT result = factory->QueryInterface(riid, ppv);
                factory->Release();

                return result;
        }
        
        LOG("CLASS_E_CLASSNOTAVAILABLE in DllGetClassObject\n");
        return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
        LOG("DllCanUnloadNow be called %u %u\n", g_lock_count, g_obj_count);
        
        if (0 == g_lock_count && 0 == g_obj_count)
                return S_OK;
        else
                return S_FALSE;
}


//-------------------------------------------------
//               PropPage implement
//-------------------------------------------------
PropPage::PropPage()
        : ref_count_(1)
{
        ++g_obj_count;
}

PropPage::~PropPage()
{
        LOG("Destroy PropPage\n");
        --g_obj_count;
}

STDMETHODIMP_(ULONG) PropPage::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) PropPage::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}


STDMETHODIMP PropPage::QueryInterface(REFIID riid, void** ppvObject)
{
        PrintIID(_T("PropPage receive"), riid);

        // Give the correct interface to client
        if (IID_IShellExtInit == riid || IID_IUnknown == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                LOG("PropPage IID_IShellExtInit or IID_IUnknown\n");
                *ppvObject = static_cast<IShellExtInit*>(this);
        } else if (IID_IShellPropSheetExt == riid) {
                LOG("PropPage IID_IShellPropSheetExt\n");
                *ppvObject = static_cast<IShellPropSheetExt*>(this);
        } else {
                LOG("PropPage E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}


STDMETHODIMP PropPage::Initialize(PCIDLIST_ABSOLUTE, 
                                  IDataObject *pdtobj, 
                                  HKEY)
{
        return DoInitialize(pdtobj, &g_prop_sheet_file_list);
}

// Adds one or more pages to a property sheet that the Shell displays for a 
// file object. The Shell calls this method for each property sheet handler 
// registered to the file type.
STDMETHODIMP PropPage::AddPages(LPFNADDPROPSHEETPAGE pfnAddPage, LPARAM lParam)
{
        HRESULT result                = S_OK;
        vector<FilePath>::iterator it = g_prop_sheet_file_list.begin();

        for (; it != g_prop_sheet_file_list.end(); it++) {
                PROPSHEETPAGE sheet_page = {};
        
                sheet_page.dwSize      = sizeof(sheet_page);
                sheet_page.dwFlags     = PSP_DEFAULT | PSP_USETITLE | PSP_USEREFPARENT;
                sheet_page.hInstance   = g_this_dll;
                sheet_page.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG1);
                sheet_page.pszTitle    = PathFindFileName(it->data);
                sheet_page.pfnDlgProc  = PropDialogProc;
                sheet_page.lParam      = reinterpret_cast<LPARAM>(&it->data);
                sheet_page.pcRefParent = reinterpret_cast<UINT*>(&g_page_count);
        
                HPROPSHEETPAGE page = CreatePropertySheetPage(&sheet_page);
        
                if (page) {
                        if (!pfnAddPage(page, lParam)) {
                                DestroyPropertySheetPage(page);
                                result = S_FALSE;
                        }
               }
        }

        if (S_OK == result)
                LOG("AddPages S_OK\n");
        else
                LOG("AddPages S_FALSE\n");

        return result;
}

STDMETHODIMP PropPage::ReplacePage(UINT, 
                                   LPFNADDPROPSHEETPAGE, 
                                   LPARAM)
{
        return E_NOTIMPL;
}


//-------------------------------------------------
//           PropPageFactory implement     
//-------------------------------------------------
PropPageFactory::PropPageFactory() 
        : ref_count_(1)
{
        ++g_obj_count;
}

PropPageFactory::~PropPageFactory()
{
        LOG("Destroy PropPageFactory\n");
        --g_obj_count;
}

STDMETHODIMP_(ULONG) PropPageFactory::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) PropPageFactory::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP PropPageFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                LOG("IID_IUnknown\n");
                *ppvObject = static_cast<IUnknown*>(this);
        } else if (IID_IClassFactory == riid) {
                LOG("IID_IClassFactory\n");
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                LOG("PropPageFactory E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

// The class factory is responsible for creating the interface
// which the client specified
STDMETHODIMP PropPageFactory::CreateInstance(IUnknown* pUnkOuter,
                                                   REFIID riid, 
                                                   void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter) {
                LOG("PropPageFactory CLASS_E_NOAGGREGATION\n");
                return CLASS_E_NOAGGREGATION;
        }

        PropPage* prop_page = new PropPage();

        HRESULT result = prop_page->QueryInterface(riid, ppvObject);
        prop_page->Release();

        return result;
}

STDMETHODIMP PropPageFactory::LockServer(BOOL fLock)
{
        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        return S_OK;
}


//-------------------------------------------------
//               ExeMenu implement
//-------------------------------------------------
ExeMenu::ExeMenu()
        : ref_count_(1)
{
        ++g_obj_count;
}

ExeMenu::~ExeMenu()
{
        LOG("Destroy ExeMenu\n");
        --g_obj_count;
}

STDMETHODIMP ExeMenu::QueryInterface(REFIID riid, void** ppvObject)
{
        PrintIID(_T("ExeMenu receive"), riid);

        // Give the correct interface to client
        if (IID_IShellExtInit == riid || IID_IUnknown == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                LOG("ExeMenu IID_IShellExtInit or IID_IUnknown\n");
                *ppvObject = static_cast<IShellExtInit*>(this);
        } else if (IID_IContextMenu == riid) {
                LOG("ExeMenu IID_IContextMenu\n");
                *ppvObject = static_cast<IContextMenu*>(this);
        } else {
                LOG("ExeMenu E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

STDMETHODIMP_(ULONG) ExeMenu::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) ExeMenu::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP ExeMenu::Initialize(PCIDLIST_ABSOLUTE,
                                 IDataObject *pdtobj, 
                                 HKEY)
{
        return DoInitialize(pdtobj, &g_menu_file_list);
}

// Gets information about a shortcut menu command, including the help string 
// and the language-independent, or canonical, name for the command
STDMETHODIMP ExeMenu::GetCommandString(UINT_PTR, 
                                       UINT uFlags,
                                       UINT*,
                                       LPSTR pszName,
                                       UINT cchMax)
{
        switch (uFlags) {
        case GCS_HELPTEXTA:
                LOG("GCS_HELPTEXTA\n");
                strncat(pszName, "exe cmd help text", cchMax - 1);
                break;
        case GCS_HELPTEXTW:
                LOG("GCS_HELPTEXTW\n");
                wcsncat(reinterpret_cast<WCHAR*>(pszName), L"exe cmd help text", cchMax - 1);
                break;
        default:
                ;
        }

        return S_OK;
}

STDMETHODIMP ExeMenu::InvokeCommand(CMINVOKECOMMANDINFO*)
{
        TCHAR buf[512] = {};
        TCHAR* source  = buf;
        int buf_space  = _countof(buf);

        vector<FilePath>::iterator it = g_menu_file_list.begin();

        for (; it != g_menu_file_list.end(); it++) {
                TCHAR* file_name = PathFindFileName(it->data);
                int len          = lstrlen(file_name);

                _tcsncat(source, file_name, buf_space - 1);
                source[len]     = _T('\n');
                source[len + 1] = _T('\0');
                source += len + 1;
                buf_space -= len + 1;
        }

        MessageBox(NULL, buf, _T("context menu test"), MB_OK);
        return S_OK;
}

// Adds commands to a shortcut menu
STDMETHODIMP ExeMenu::QueryContextMenu(HMENU hmenu, 
                                       UINT indexMenu,
                                       UINT idCmdFirst,
                                       UINT,
                                       UINT)
{
        UINT cmd_id = idCmdFirst;

        InsertMenu(hmenu, indexMenu, MF_STRING | MF_BYPOSITION, cmd_id++, _T("shell ext test"));

        return MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_NULL, cmd_id - idCmdFirst);
}


//-------------------------------------------------
//             CopyMonitor implement
//-------------------------------------------------
CopyMonitor::CopyMonitor()
        : ref_count_(1)
{
        ++g_obj_count;
}

CopyMonitor::~CopyMonitor()
{
        LOG("Destroy ExeMenu\n");
        --g_obj_count;
}

STDMETHODIMP CopyMonitor::QueryInterface(REFIID riid, void** ppvObject)
{
        PrintIID(_T("CopyMonitor receive"), riid);

        // Give the correct interface to client
        if (IID_IUnknown == riid || IID_IShellCopyHook == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                LOG("CopyMonitor IID_IUnknown or IID_IShellCopyHook\n");
                *ppvObject = static_cast<ICopyHook*>(this);
        } else {
                LOG("CopyMonitor E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

STDMETHODIMP_(ULONG) CopyMonitor::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) CopyMonitor::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP_(UINT) CopyMonitor::CopyCallback(HWND,
                                              UINT wFunc,
                                              UINT,
                                              LPCTSTR pszSrcFile,
                                              DWORD,
                                              LPCTSTR pszDestFile,
                                              DWORD)
{
        TCHAR buf[64] = {};

        GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, buf, _countof(buf));
        switch (wFunc) {
        case FO_COPY:
                LOG("%ls\tCopying: %ls to %ls\n\n", buf, pszSrcFile, pszDestFile);
                break;
        case FO_DELETE:
                LOG("%ls\tDeleting: %ls to %ls\n\n", buf, pszSrcFile, pszDestFile);
                break;
        case FO_MOVE:
                LOG("%ls\tMoving: %ls to %ls\n\n", buf, pszSrcFile, pszDestFile);
                break;
        case FO_RENAME:
                LOG("%ls\tRenaming: %ls to %ls\n\n", buf, pszSrcFile, pszDestFile);
                break;
        default:
                ;
        }

        return IDYES;
}


//-------------------------------------------------
//               DropTxt implement
//-------------------------------------------------
DropTxt::DropTxt()
        : ref_count_(1)
{
        ++g_obj_count;
}

DropTxt::~DropTxt()
{
        LOG("Destroy DropTxt\n");
        --g_obj_count;
}

STDMETHODIMP DropTxt::QueryInterface(REFIID riid, void** ppvObject)
{
        PrintIID(_T("DropTxt receive"), riid);

        // Give the correct interface to client
        if (IID_IUnknown == riid || IID_IDropTarget == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                LOG("DropTxt IID_IUnknown or IID_IDropTarget\n");
                *ppvObject = static_cast<IDropTarget*>(this);
        } else if (IID_IPersistFile == riid) {
                LOG("DropTxt IID_IPersistFile\n");
                *ppvObject = static_cast<IPersistFile*>(this);
        } else {
                LOG("DropTxt E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

STDMETHODIMP_(ULONG) DropTxt::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) DropTxt::Release()
{
        ULONG res = --ref_count_;
        LOG("DropTxt Release count=%u\n", res);
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP DropTxt::DragEnter(IDataObject *pDataObj,
                                DWORD,
                                POINTL,
                                DWORD *pdwEffect)
{
        STGMEDIUM medium = {};
        FORMATETC fe     = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
        
        if (FAILED(pDataObj->GetData(&fe, &medium))) {
               *pdwEffect = DROPEFFECT_NONE;
               LOG("DragEnter get data fail!\n");
               return E_UNEXPECTED;
        }
        
        *pdwEffect = DROPEFFECT_COPY;
        ReleaseStgMedium(&medium);

        LOG("DragEnter get data success!\n");
        return S_OK;
}

STDMETHODIMP DropTxt::DragLeave()
{
        return S_OK;
}

STDMETHODIMP DropTxt::DragOver(DWORD, POINTL, DWORD *pdwEffect)
{
        *pdwEffect = DROPEFFECT_COPY;
        return S_OK;
}

STDMETHODIMP DropTxt::Drop(IDataObject *pDataObj,
                           DWORD,
                           POINTL,
                           DWORD*)
{
        STGMEDIUM medium = {};
        FORMATETC fe     = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
 
        if (FAILED(pDataObj->GetData(&fe, &medium))) {
                // User drap plain text
                FORMATETC fe2 = {CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};

                memset(&medium, 0, sizeof(medium));
                if (FAILED(pDataObj->GetData(&fe2, &medium)))
                        return E_INVALIDARG;

                // copy content from source to target
                TCHAR* text       = static_cast<TCHAR*>(GlobalLock(medium.hGlobal));
                FILE* target_file = _wfopen(target_path_, L"a");
                
                if (!target_file) {
                        LOG("open file fail\n");
                        PopupError();
                        return E_UNEXPECTED;
                }

                _ftprintf(target_file, _T("%ls"), text);
                fclose(target_file);
                GlobalUnlock(medium.hGlobal);
        } else {
                // User drap files
                HDROP drop           = static_cast<HDROP>(medium.hGlobal);
                TCHAR path[MAX_PATH] = {};
                 
                DragQueryFile(drop, 0, path, _countof(path));
                DragFinish(drop);
                LOG("receive drop file\n%ls\n", path);
                
                TCHAR* ext = PathFindExtension(path);

                if (_tcsnccmp(_T(".txt"), ext, 4)) {
                        MessageBox(NULL, 
                                   _T("Sorry, but you can only drap txt files."),
                                   _T("Drap files..."), 
                                   MB_OK);

                        return E_INVALIDARG;
                }

                // copy content from source to target
                FILE* target_file = _wfopen(target_path_, L"a");
                FILE* source_file = _tfopen(path, _T("r"));

                if (!target_file || !source_file) {
                        LOG("open file fail, target=%p source=%p\n", target_file, source_file);
                        PopupError();
                        return E_UNEXPECTED;
                }
                
                TCHAR buf[512] = {};
                while (_fgetts(buf, _countof(buf), source_file))
                        _ftprintf(target_file, _T("%ls"), buf);

                fclose(target_file);
                fclose(source_file);
        }
 
        return S_OK;
}

STDMETHODIMP DropTxt::GetClassID(CLSID *pClassID)
{
        *pClassID = CLSID_DropTxt;
        return S_OK;
}

STDMETHODIMP DropTxt::GetCurFile(LPOLESTR*)
{
        return E_NOTIMPL;
}

STDMETHODIMP DropTxt::IsDirty()
{
        return E_NOTIMPL;
}

STDMETHODIMP DropTxt::Load(LPCOLESTR pszFileName, DWORD)
{
        LOG("receive target file:\n%ls\n", pszFileName);
        wcsncat(target_path_, pszFileName, _countof(target_path_));
        return S_OK;
}

STDMETHODIMP DropTxt::Save(LPCOLESTR, BOOL)
{
        return E_NOTIMPL;
}

STDMETHODIMP DropTxt::SaveCompleted(LPCOLESTR)
{
        return E_NOTIMPL;
}


//-------------------------------------------------
//           ExeMenuFactory implement     
//-------------------------------------------------
ExeMenuFactory::ExeMenuFactory() 
        : ref_count_(1)
{
        ++g_obj_count;
}

ExeMenuFactory::~ExeMenuFactory()
{
        LOG("Destroy ExeMenuFactory\n");
        --g_obj_count;
}

STDMETHODIMP_(ULONG) ExeMenuFactory::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) ExeMenuFactory::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP ExeMenuFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                LOG("ExeMenuFactory IID_IUnknown\n");
                *ppvObject = static_cast<IUnknown*>(this);
        } else if (IID_IClassFactory == riid) {
                LOG("ExeMenuFactory IID_IClassFactory\n");
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                LOG("ExeMenuFactory E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

// The class factory is responsible for creating the interface
// which the client specified
STDMETHODIMP ExeMenuFactory::CreateInstance(IUnknown* pUnkOuter,
                                                   REFIID riid, 
                                                   void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter) {
                LOG("ExeMenuFactory CLASS_E_NOAGGREGATION\n");
                return CLASS_E_NOAGGREGATION;
        }

        ExeMenu* exe_menu = new ExeMenu();

        HRESULT result = exe_menu->QueryInterface(riid, ppvObject);
        exe_menu->Release();

        return result;
}

STDMETHODIMP ExeMenuFactory::LockServer(BOOL fLock)
{
        LOG("ExeMenuFactory::LockServer fLock=%d\n", fLock);

        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        return S_OK;
}


//-------------------------------------------------
//         CopyMonitorFactory implement     
//-------------------------------------------------
CopyMonitorFactory::CopyMonitorFactory() 
        : ref_count_(1)
{
        ++g_obj_count;
}

CopyMonitorFactory::~CopyMonitorFactory()
{
        LOG("Destroy CopyMonitorFactory\n");
        --g_obj_count;
}

STDMETHODIMP_(ULONG) CopyMonitorFactory::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) CopyMonitorFactory::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP CopyMonitorFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                LOG("CopyMonitorFactory IID_IUnknown\n");
                *ppvObject = static_cast<IUnknown*>(this);
        } else if (IID_IClassFactory == riid) {
                LOG("CopyMonitorFactory IID_IClassFactory\n");
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                LOG("CopyMonitorFactory E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

// The class factory is responsible for creating the interface
// which the client specified
STDMETHODIMP CopyMonitorFactory::CreateInstance(IUnknown* pUnkOuter,
                                                   REFIID riid, 
                                                   void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter) {
                LOG("CopyMonitorFactory CLASS_E_NOAGGREGATION\n");
                return CLASS_E_NOAGGREGATION;
        }

        CopyMonitor* copy_monitor = new CopyMonitor();

        HRESULT result = copy_monitor->QueryInterface(riid, ppvObject);
        copy_monitor->Release();

        return result;
}

STDMETHODIMP CopyMonitorFactory::LockServer(BOOL fLock)
{
        LOG("CopyMonitorFactory::LockServer fLock=%d\n", fLock);

        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        return S_OK;
}


//-------------------------------------------------
//           DropTxtFactory implement     
//-------------------------------------------------

DropTxtFactory::DropTxtFactory()
        : ref_count_(1)
{
        ++g_obj_count;
}

DropTxtFactory::~DropTxtFactory()
{
        LOG("Destroy DropTxtFactory\n");
        --g_obj_count;
}

STDMETHODIMP DropTxtFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                LOG("DropTxtFactory IID_IUnknown\n");
                *ppvObject = static_cast<IUnknown*>(this);
        } else if (IID_IClassFactory == riid) {
                LOG("DropTxtFactory IID_IClassFactory\n");
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                LOG("DropTxtFactory E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

STDMETHODIMP_(ULONG) DropTxtFactory::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) DropTxtFactory::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP DropTxtFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter) {
                LOG("DropTxtFactory CLASS_E_NOAGGREGATION\n");
                return CLASS_E_NOAGGREGATION;
        }

        DropTxt* drop_txt = new DropTxt();

        HRESULT result = drop_txt->QueryInterface(riid, ppvObject);
        drop_txt->Release();

        return result;
}

STDMETHODIMP DropTxtFactory::LockServer(BOOL fLock)
{
        LOG("DropTxtFactory::LockServer fLock=%d\n", fLock);

        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        return S_OK;
}
