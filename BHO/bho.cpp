#define _CRT_SECURE_NO_WARNINGS

#define NDEBUG

#include "bho.h"
#include "helper.h"
#include "com_helper.h"
#include "IUnknownImpl.h"
#include "ComPtr.h"
#include "hook.h"
#include <cstdio>
#include <clocale>
#include <Guiddef.h>
#include <initguid.h>
#include <Shlwapi.h>
#include <Exdisp.h>
#include <Exdispid.h>


namespace {

HINSTANCE g_this_dll      = NULL;
ULONG g_lock_count        = 0;

// {C3CBBFE0-0EE8-42b1-AB46-B20725DFF664}
DEFINE_GUID(CLSID_UrlMonitor, 
0XC3CBBFE0, 0XEE8, 0X42B1, 0XAB, 0X46, 0XB2, 0X7, 0X25, 0XDF, 0XF6, 0X64);

void DoWriteFile(HANDLE handle, const void* buffer, size_t buffer_size)
{
        DWORD bytes_write = 0;

        BOOL result = WriteFile(handle,
                                buffer,
                                buffer_size,
                                &bytes_write,
                                NULL);
        if (!result) {
                LOG("WriteFile fail! handle=%x error=%u\n", handle, GetLastError());
                return;
        }

        if (bytes_write != buffer_size)
                LOG("WriteFile write only %u bytes\n", bytes_write);
}

bool SendHttpData(HINTERNET http_handle, HttpStatus status, size_t port, const TCHAR* url)
{
        bool ret         = false;
        TCHAR* pipe_name = _T("\\\\.\\pipe\\hook_http_pipe");
        HANDLE pipe      = INVALID_HANDLE_VALUE;
        
        // We should use a loop to wait because the pipe server is busy
        // sometimes
        for (;;) {
                // Try to connect to pipe server
                pipe = CreateFile(pipe_name,
                                  GENERIC_WRITE,
                                  0,
                                  NULL,
                                  OPEN_EXISTING,
                                  0,
                                  NULL);

                if (INVALID_HANDLE_VALUE != pipe) break;

                if (ERROR_FILE_NOT_FOUND == GetLastError()) break;

                if (ERROR_PIPE_BUSY == GetLastError())
                        WaitNamedPipe(pipe_name, NMPWAIT_WAIT_FOREVER);
        }

        if (INVALID_HANDLE_VALUE != pipe) {
                HttpInfo http_info = {};

                http_info.http_handle_ = http_handle;
                http_info.status_      = status;
                http_info.port_        = port;
                
                if (url)
                        http_info.data_len_ = lstrlen(url);
                
                DoWriteFile(pipe, &http_info, sizeof(http_info));

                if (url)
                        DoWriteFile(pipe, url, sizeof(TCHAR) * http_info.data_len_);
                
                CloseHandle(pipe);
                ret = true;
        }

        return ret;
}

bool LoadedByIE()
{
        TCHAR path[MAX_PATH] = {};
        const TCHAR* ie_path = _T("C:\\Program Files\\Internet Explorer\\iexplore.exe");

        GetModuleFileName(NULL, path, MAX_PATH);

        return (0 == lstrcmpi(path, ie_path)) ? true : false;
}

HINTERNET WINAPI HookHttpOpenRequestA(HINTERNET hConnect,
                                      LPCSTR lpszVerb,
                                      LPCSTR lpszObjectName,
                                      LPCSTR lpszVersion,
                                      LPCSTR lpszReferer,
                                      LPCSTR FAR * lplpszAcceptTypes,
                                      DWORD dwFlags,
                                      DWORD_PTR dwContext)
{
        LOG("\nHookHttpOpenRequestA called!\nUrl=%s\nVerb=%s\n"
            "Version=%s\nReferer=%s\n",
            lpszObjectName,
            lpszVerb,
            lpszVersion,
            lpszReferer);
        
        return HttpOpenRequestA(hConnect,
                                lpszVerb,
                                lpszObjectName,
                                lpszVersion,
                                lpszReferer,
                                lplpszAcceptTypes,
                                dwFlags,
                                dwContext);
}

HINTERNET WINAPI HookHttpOpenRequestW(HINTERNET hConnect,
                                      LPCWSTR lpszVerb,
                                      LPCWSTR lpszObjectName,
                                      LPCWSTR lpszVersion,
                                      LPCWSTR lpszReferer,
                                      LPCWSTR FAR * lplpszAcceptTypes,
                                      DWORD dwFlags,
                                      DWORD_PTR dwContext)
{
//        LOG("\nHookHttpOpenRequestW called! %x\nUrl=%ls\nVerb=%ls\n"
//            "Version=%ls\nReferer=%ls\n",
//            hConnect,
//            lpszObjectName,
//            lpszVerb,
//            lpszVersion,
//            lpszReferer);
        SendHttpData(hConnect, OpenRequest, 0, lpszObjectName);
        return HttpOpenRequestW(hConnect,
                                lpszVerb,
                                lpszObjectName,
                                lpszVersion,
                                lpszReferer,
                                lplpszAcceptTypes,
                                dwFlags,
                                dwContext);
}

HINTERNET WINAPI HookInternetConnectA(HINTERNET hInternet,
                                      LPCSTR lpszServerName,
                                      INTERNET_PORT nServerPort,
                                      LPCSTR lpszUserName,
                                      LPCSTR lpszPassword,
                                      DWORD dwService,
                                      DWORD dwFlags,
                                      DWORD_PTR dwContext)
{
        LOG("\nHookInternetConnectA be called!\nUrl=%s\nPort=%d\n",
            lpszServerName,
            nServerPort);

        return InternetConnectA(hInternet,
                                lpszServerName,
                                nServerPort,
                                lpszUserName,
                                lpszPassword,
                                dwService,
                                dwFlags,
                                dwContext);
}

HINTERNET WINAPI HookInternetConnectW(HINTERNET hInternet,
                                      LPCWSTR lpszServerName,
                                      INTERNET_PORT nServerPort,
                                      LPCWSTR lpszUserName,
                                      LPCWSTR lpszPassword,
                                      DWORD dwService,
                                      DWORD dwFlags,
                                      DWORD_PTR dwContext)
{
        HINTERNET ret = InternetConnectW(hInternet,
                                         lpszServerName,
                                         nServerPort,
                                         lpszUserName,
                                         lpszPassword,
                                         dwService,
                                         dwFlags,
                                         dwContext);

//        LOG("\nHookInternetConnectW be called! %x\nUrl=%ls\nPort=%d\n",
//            ret,
//            lpszServerName,
//            nServerPort);
        SendHttpData(ret, Connect, nServerPort, lpszServerName);
        return ret;
}

BOOL WINAPI HookFreeLibrary(HMODULE hModule)
{
//        TCHAR buf[MAX_PATH] = {};

//        GetModuleFileName(hModule, buf, _countof(buf));
//        LOG("\nFreeLibrary %ls\n", PathFindFileName(buf));

        return FreeLibrary(hModule);
}

BOOL WINAPI HookInternetCloseHandle(HINTERNET hInternet)
{
//        LOG("HookInternetCloseHandle be called %x\n", hInternet);
        SendHttpData(hInternet, Close, 0, NULL);
        return InternetCloseHandle(hInternet);
}


void DoHook()
{
        InitHookModule(g_this_dll);
        
        // IE use urlmon to send http request, the urlmon.dll use delay load
        // for wininet.dll
        DelayAPIHook("Wininet.dll", "HttpOpenRequestW", (FARPROC)HookHttpOpenRequestW);
        DelayAPIHook("Wininet.dll", "InternetConnectW", (FARPROC)HookInternetConnectW);
        DelayAPIHook("Wininet.dll", "InternetCloseHandle", (FARPROC)HookInternetCloseHandle);
}

void HandleBeforeNavigate(DISPPARAMS FAR* pDispParams)
{
        LOG("DISPID_BEFORENAVIGATE2\n");
        LOG("num args=%u named num args=%u\n",
            pDispParams->cArgs,
            pDispParams->cNamedArgs);
        
        VARIANTARG args[7] = {};

        for (int i = 0; i < _countof(args); i++)
                VariantInit(&args[i]);
        
        HRESULT result = VariantChangeType(&args[0], &pDispParams->rgvarg[5], 0, VT_BSTR);
        if (SUCCEEDED(result))
                LOG("url=%ls\n", args[0].bstrVal);

        result = VariantChangeType(&args[1], &pDispParams->rgvarg[3], 0, VT_BSTR);
        if (SUCCEEDED(result))
                LOG("window=%ls\n", args[1].bstrVal);
        
        result = VariantChangeType(&args[2], &pDispParams->rgvarg[1], 0, VT_BSTR);
        if (SUCCEEDED(result))
                LOG("header=%ls\n\n", args[2].bstrVal);

        for (int i = 0; i < _countof(args); i++)
                VariantClear(&args[i]);
}


//-------------------------------------------------
//                  COM classes
//-------------------------------------------------
class UrlMonitor : public IUnknownImpl, public IObjectWithSite, public IDispatch {
public:
        UrlMonitor();
        virtual ~UrlMonitor();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP SetSite(IUnknown* pUnkSite);
        STDMETHODIMP GetSite(REFIID riid, void** ppvSite);

        STDMETHODIMP GetTypeInfoCount(unsigned int FAR* pctinfo);

        STDMETHODIMP GetTypeInfo(unsigned int iTInfo,
                                 LCID lcid,
                                 ITypeInfo FAR* FAR* ppTInfo);

        STDMETHODIMP GetIDsOfNames(REFIID riid,                  
                                   OLECHAR FAR* FAR* rgszNames,  
                                   unsigned int cNames,          
                                   LCID lcid,                   
                                   DISPID FAR* rgDispId);
 
        STDMETHODIMP Invoke(DISPID dispIdMember,      
                            REFIID riid,              
                            LCID lcid,                
                            WORD wFlags,              
                            DISPPARAMS FAR* pDispParams,  
                            VARIANT FAR* pVarResult,  
                            EXCEPINFO FAR* pExcepInfo,  
                            unsigned int FAR* puArgErr);
private:
        void Connect(IUnknown* pUnkSite);
        void ReleaseResources();
        
        IWebBrowser2* web_browser_;
        DWORD cookie_;
        bool resource_released_;
};


//-------------------------------------------------
//               COM class factory
//-------------------------------------------------
class UrlMonitorFactory : public IUnknownImpl, public IClassFactory {
public:
        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);
};

}       // End of unnamed namespace


//-------------------------------------------------
//             UrlMonitor implement
//-------------------------------------------------
UrlMonitor::UrlMonitor() 
        : web_browser_(NULL)
        , cookie_(0)
        , resource_released_(false)
{
}

UrlMonitor::~UrlMonitor()
{
        LOG("dtor of UrlMonitor\n");
        ReleaseResources();
}

STDMETHODIMP_(ULONG) UrlMonitor::AddRef()
{
        return IUnknownImpl::AddRef();
}

STDMETHODIMP_(ULONG) UrlMonitor::Release()
{
        return IUnknownImpl::Release();
}

STDMETHODIMP UrlMonitor::QueryInterface(REFIID riid, void** ppvObject)
{
        //PrintIID(_T("UrlMonitor receive"), riid);

        // Give the correct interface to client
        if (IID_IObjectWithSite == riid || IID_IUnknown == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                LOG("UrlMonitor IID_IObjectWithSite or IID_IUnknown\n");
                *ppvObject = static_cast<IObjectWithSite*>(this);
        } else if (IID_IDispatch == riid) {
//                LOG("UrlMonitor IID_IDispatch\n");
                *ppvObject = static_cast<IDispatch*>(this);
        } else {
//                LOG("UrlMonitor E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

void UrlMonitor::Connect(IUnknown* pUnkSite)
{
        IConnectionPointContainer* container = NULL;
        IConnectionPoint* point              = NULL;

        HRESULT result = pUnkSite->QueryInterface(IID_IConnectionPointContainer,
                                                  reinterpret_cast<void**>(&container));
        if (FAILED(result))
                LOG("query IConnectionPointContainer fail!\n");

        result = container->FindConnectionPoint(DIID_DWebBrowserEvents2,
                                                &point);
        if (FAILED(result))
                LOG("FindConnectionPoint fail!\n"); 

        result = point->Advise(reinterpret_cast<IUnknown*>(this), &cookie_);
        if (FAILED(result))
                LOG("Advise fail!"); 
}

void UrlMonitor::ReleaseResources()
{
        if (resource_released_) return;
        
        if (web_browser_) {
                web_browser_->Release();
                web_browser_ = NULL;
        }

        DeinitHookModule();

        resource_released_ = true;
}

STDMETHODIMP UrlMonitor::SetSite(IUnknown* pUnkSite)
{
        static bool not_hooked = true;

        LOG("SetSite\n");
        if (pUnkSite) {
                // We don't know how many times IE will invoke this method,
                // so need to make sure only call DoHook only one time.
                if (not_hooked) {
                        DoHook();
                        not_hooked = false;
                }

                IWebBrowser2* tmp = NULL;
                HRESULT result    = pUnkSite->QueryInterface(IID_IWebBrowser2,
                                                             reinterpret_cast<void**>(&tmp));
                if (SUCCEEDED(result)) {
                        if (web_browser_)
                                web_browser_->Release();
                        
                        web_browser_ = tmp;
                }
                
                Connect(pUnkSite);
                
                // We can not know how many times the IE will invoke this method,
                // so need to make sure only call DoHook only one time.
        } else {
                LOG("SetSite with NULL argument!\n");
                ReleaseResources();
        }

        return S_OK;
}

STDMETHODIMP UrlMonitor::GetSite(REFIID riid, void** ppvSite)
{
        PrintIID(_T("GetSite with iid:"), riid);

        if (web_browser_) {
                HRESULT result = web_browser_->QueryInterface(riid, ppvSite);

                if (SUCCEEDED(result))
                        return S_OK;
                else
                        return E_NOINTERFACE;
        } else {
                LOG("GetSite when web_browser_ is NULL");
                *ppvSite = NULL;
                return E_FAIL;
        }
}

STDMETHODIMP UrlMonitor::GetTypeInfoCount(unsigned int FAR* pctinfo)
{
        LOG("GetTypeInfoCount\n");
        if (!pctinfo) {
                LOG("pctinfo is NULL!\n");
                return E_INVALIDARG;
        }

        *pctinfo = 0;
        return S_OK;
}

STDMETHODIMP UrlMonitor::GetTypeInfo(unsigned int,
                                     LCID,
                                     ITypeInfo FAR* FAR*)
{
        LOG("GetTypeInfo\n");
        return E_NOTIMPL;
}

STDMETHODIMP UrlMonitor::GetIDsOfNames(REFIID,                  
                                       OLECHAR FAR* FAR*,  
                                       unsigned int,          
                                       LCID,                   
                                       DISPID FAR*)
{
        LOG("GetIDsOfNames\n");
        return E_NOTIMPL;
}

STDMETHODIMP UrlMonitor::Invoke(DISPID dispIdMember,      
                                REFIID,                      // Reserved, must be NULL
                                LCID,                
                                WORD,              
                                DISPPARAMS FAR* pDispParams,  
                                VARIANT FAR*,  
                                EXCEPINFO FAR*,  
                                unsigned int FAR*)
{
        if (!pDispParams) return E_INVALIDARG;
        
        switch (dispIdMember) {
        case DISPID_BEFORENAVIGATE2:
                //HandleBeforeNavigate(pDispParams);
                break;
        default:
                ;
        }
        return S_OK;
}


//-------------------------------------------------
//          UrlMonitorFactory implement     
//-------------------------------------------------
STDMETHODIMP UrlMonitorFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                LOG("IID_IUnknown\n");
                *ppvObject = static_cast<IUnknownImpl*>(this);
        } else if (IID_IClassFactory == riid) {
                LOG("IID_IClassFactory\n");
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                LOG("UrlMonitorFactory E_NOINTERFACE\n");
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

STDMETHODIMP_(ULONG) UrlMonitorFactory::AddRef()
{
        return IUnknownImpl::AddRef();
}

STDMETHODIMP_(ULONG) UrlMonitorFactory::Release()
{
        return IUnknownImpl::Release();
}

// The class factory is responsible for creating the interface
// which the client specified
STDMETHODIMP UrlMonitorFactory::CreateInstance(IUnknown* pUnkOuter,
                                               REFIID riid,
                                               void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter) {
                LOG("PropPageFactory CLASS_E_NOAGGREGATION\n");
                return CLASS_E_NOAGGREGATION;
        }
        
        UrlMonitor* url_monitor = new UrlMonitor();
        
        HRESULT result = url_monitor->QueryInterface(riid, ppvObject);
        url_monitor->Release();

        return result;
}

STDMETHODIMP UrlMonitorFactory::LockServer(BOOL fLock)
{
        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        return S_OK;
}


//-------------------------------------------------
//                DLL entry point
//-------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
        static char* old_locale = NULL;
        char log_path[MAX_PATH] = {};
        int offset = 0;

        switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
                g_this_dll = hinstDLL;
                
                if (LoadedByIE()) {
                        old_locale = setlocale(LC_CTYPE, "");
                        // Get the location of this dll
                        GetModuleFileNameA(hinstDLL, log_path, _countof(log_path));
                        PathRemoveFileSpecA(log_path);
                        offset = strlen(log_path);
        
                        _snprintf(&log_path[offset],
                                  sizeof(log_path) - offset - 1,
                                  "\\log-%u.txt",
                                  GetCurrentProcessId());
        
                        InitLogFile(log_path);
                        LOG("BHO DLL_PROCESS_ATTACH\n");
                }
                
                // We disable the DLL_THREAD_ATTACH, DLL_THREAD_DETACH notification
                // for our dll to avoid browser performance impact
                if (!DisableThreadLibraryCalls(hinstDLL))
                        LOG("DisableThreadLibraryCalls fail!");
                
                break;
        case DLL_PROCESS_DETACH:
                if (LoadedByIE()) {
                        LOG("BHO DLL_PROCESS_DETACH\n");
                        CloseLogFile();
                        setlocale(LC_CTYPE, old_locale);
                }

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
        if (CLSID_UrlMonitor == rclsid) {
                IClassFactory* factory = new UrlMonitorFactory();
                HRESULT result         = factory->QueryInterface(riid, ppv);

                factory->Release();
                return result;
        }

        LOG("CLASS_E_CLASSNOTAVAILABLE in DllGetClassObject\n");
        return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
        LOG("DllCanUnloadNow be called %u\n", g_lock_count);
        
        if (0 == g_lock_count)
                return S_OK;
        else
                return S_FALSE;
}

STDAPI DllRegisterServer()
{
        TCHAR clsid[]        = _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}");
        TCHAR prog_id[]      = _T("bho.UrlMonitor");
        TCHAR thread_model[] = _T("Apartment");
        DWORD no_explorer    = 1;
        TCHAR path[MAX_PATH] = {};        
        
        // Get the location of this dll
        GetModuleFileName(g_this_dll, path, _countof(path));

        // Register CLSID
        LSTATUS result = SHSetValue(HKEY_LOCAL_MACHINE, 
                                    _T("SOFTWARE\\Classes\\bho.UrlMonitor\\CLSID"),
                                    NULL, REG_SZ, clsid, sizeof(clsid));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Classes\\CLSID\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}\\ProgID"),
                            NULL, REG_SZ, prog_id, sizeof(prog_id));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Classes\\CLSID\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}\\InProcServer32"),
                            NULL, REG_SZ, path, sizeof(path));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Classes\\CLSID\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}\\")
                            _T("InProcServer32"),
                            _T("ThreadingModel"), REG_SZ, thread_model, sizeof(thread_model));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        // Register BHO entry
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\")
                            _T("Explorer\\Browser Helper Objects\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}"),
                            NULL, REG_SZ, _T(""), 0);
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\")
                            _T("Explorer\\Browser Helper Objects\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}"),
                            _T("NoExplorer"), REG_DWORD, &no_explorer, sizeof(no_explorer));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        return S_OK;
}

STDAPI DllUnregisterServer()
{
        LSTATUS result = SHDeleteKey(HKEY_LOCAL_MACHINE, 
                                     _T("SOFTWARE\\Classes\\bho.UrlMonitor"));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHDeleteKey(HKEY_LOCAL_MACHINE, 
                             _T("SOFTWARE\\Classes\\CLSID\\{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}"));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        result = SHDeleteKey(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\")
                            _T("Explorer\\Browser Helper Objects\\")
                            _T("{C3CBBFE0-0EE8-42B1-AB46-B20725DFF664}"));
        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        return S_OK;
}
