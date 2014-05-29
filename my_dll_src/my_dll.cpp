#define _CRT_SECURE_NO_WARNINGS

#include "my_dll.h"
#include <initguid.h>
#include "my_dll_iid.h"
#include <cstdio>
#include <cassert>
#include <Shlwapi.h>
#include <Olectl.h>

namespace {

//-------------------------------------------------
//                   COM class
//-------------------------------------------------
class SimpleCom : public ISimpleCom, public ISimpleCom2 {
public:
        SimpleCom();
        virtual ~SimpleCom();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        virtual STDMETHODIMP foo(BSTR str);
        virtual STDMETHODIMP bar(BSTR str);
private:
        ULONG ref_count_;
};


//-------------------------------------------------
//               COM class factory
//-------------------------------------------------
class SimpleComClassFactory : public IClassFactory {
public:
        SimpleComClassFactory();
        virtual ~SimpleComClassFactory();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObject);
        STDMETHODIMP LockServer(BOOL fLock);

private:
        ULONG ref_count_;
};


ULONG g_lock_count = 0;
ULONG g_obj_count  = 0;

int g_test         = 0;

}       // End of unnamed namespace


//-------------------------------------------------
//                DLL entry point
//-------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
        FILE* f          = NULL;
        LPOLESTR prog_id = NULL;
        HRESULT result   = 0;

        switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
                g_test = 300;
                
                f = fopen("C:\\Documents and Settings\\lausai\\My Documents\\"
                          "Dropbox\\my_code\\windows_shell\\com.txt", "w");
                fputs("my_dll DLL_PROCESS_ATTACH\n", f);

                result = ProgIDFromCLSID(CLSID_SimpleCom, &prog_id);
                if (REGDB_E_CLASSNOTREG == result) {
                        fputs("REGDB_E_CLASSNOTREG\n", f);
                } else if (REGDB_E_READREGDB == result) {
                        fputs("REGDB_E_READREGDB\n", f);
                } else {
                        _ftprintf(f, _T("prog id=%ls\n"), prog_id);
                        CoTaskMemFree(prog_id);
                }

                fclose(f);
                break;
        case DLL_PROCESS_DETACH:
                //fputs("my_dll DLL_PROCESS_DETACH\n", f);
                break;
        default:
                ;
        }

        return TRUE;
}


//-------------------------------------------------
//             simple COM entry point
//-------------------------------------------------
// STDAPI means extern "C" HRESULT __stdcall
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
        if (CLSID_SimpleCom == rclsid) {
                SimpleComClassFactory* factory = new SimpleComClassFactory();
                
                HRESULT result = factory->QueryInterface(riid, ppv);
                factory->Release();

                return result;
        }

        return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow()
{
        if (0 == g_lock_count && 0 == g_obj_count)
                return S_OK;
        else
                return S_FALSE;
}

STDAPI DllRegisterServer()
{
        TCHAR clsid[]   = _T("{E91338F7-2E82-4397-B94E-4A99AEC690D0}");
        TCHAR prog_id[] = _T("my_dll.SimpleCom");
        TCHAR path[]    = _T("C:\\Documents and Settings\\lausai\\My Documents\\")
                          _T("Dropbox\\my_code\\windows_shell\\my_dll.dll");

        LSTATUS result = SHSetValue(HKEY_LOCAL_MACHINE, 
                                    _T("SOFTWARE\\Classes\\my_dll.SimpleCom\\CLSID"),
                                    NULL, REG_SZ, clsid, sizeof(clsid));

        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Classes\\CLSID\\")
                            _T("{E91338F7-2E82-4397-B94E-4A99AEC690D0}"),
                            NULL, REG_SZ, prog_id, sizeof(prog_id));

        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHSetValue(HKEY_LOCAL_MACHINE,
                            _T("SOFTWARE\\Classes\\CLSID\\")
                            _T("{E91338F7-2E82-4397-B94E-4A99AEC690D0}\\InProcServer32"),
                            NULL, REG_SZ, path, sizeof(path));

        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        return S_OK;
}

STDAPI DllUnregisterServer()
{
        LSTATUS result = SHDeleteKey(HKEY_LOCAL_MACHINE, 
                                     _T("SOFTWARE\\Classes\\my_dll.SimpleCom"));

        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;
        
        result = SHDeleteKey(HKEY_LOCAL_MACHINE, 
                             _T("SOFTWARE\\Classes\\CLSID\\{E91338F7-2E82-4397-B94E-4A99AEC690D0}"));

        if (ERROR_SUCCESS != result)
                return SELFREG_E_CLASS;

        return S_OK;
}


//-------------------------------------------------
//              COM class implement     
//-------------------------------------------------
SimpleCom::SimpleCom() : ref_count_(1)
{
        ++g_obj_count;
}

SimpleCom::~SimpleCom()
{
        --g_obj_count;
}

STDMETHODIMP_(ULONG) SimpleCom::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) SimpleCom::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP SimpleCom::QueryInterface(REFIID riid, void** ppvObject)
{
        // Give the correct interface to client
        if (IID_IUnknown == riid) {
                // We can't convert to type IUnknown* because
                // that will cause a compile error.
                // So choose a interface to convert to.
                *ppvObject = static_cast<ISimpleCom*>(this);
        } else if (IID_ISimpleCom == riid) {
                *ppvObject = static_cast<ISimpleCom*>(this);
        } else if (IID_ISimpleCom2 == riid) {
                *ppvObject = static_cast<ISimpleCom2*>(this);
        } else {
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }
        
        AddRef();
        return S_OK;
}

STDMETHODIMP SimpleCom::foo(BSTR)
{
        printf("kerker foo test=%d\n", g_test);
        return S_OK;
}

STDMETHODIMP SimpleCom::bar(BSTR)
{
        puts("bar");
        return S_OK;
}


//-------------------------------------------------
//          COM class factory implement     
//-------------------------------------------------
SimpleComClassFactory::SimpleComClassFactory() : ref_count_(1)
{
        ++g_obj_count;
}

SimpleComClassFactory::~SimpleComClassFactory()
{
        --g_obj_count;
}

STDMETHODIMP_(ULONG) SimpleComClassFactory::AddRef()
{
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) SimpleComClassFactory::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;

        return res;
}

STDMETHODIMP SimpleComClassFactory::QueryInterface(REFIID riid, void** ppvObject)
{
        if (IID_IUnknown == riid) {
                *ppvObject = static_cast<IUnknown*>(this);
        } else if (IID_IClassFactory == riid) {
                *ppvObject = static_cast<IClassFactory*>(this);
        } else {
                *ppvObject = NULL;
                return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
}

// The class factory is responsible for creating the interface
// which the client specified
STDMETHODIMP SimpleComClassFactory::CreateInstance(IUnknown* pUnkOuter,
                                                   REFIID riid, 
                                                   void** ppvObject)
{
        // We don't support aggregation
        if (NULL != pUnkOuter)
                return CLASS_E_NOAGGREGATION;

        SimpleCom* simple_com = new SimpleCom();

        HRESULT result = simple_com->QueryInterface(riid, ppvObject);
        simple_com->Release();

        return result;
}

STDMETHODIMP SimpleComClassFactory::LockServer(BOOL fLock)
{
        if (fLock)
                ++g_lock_count;
        else
                --g_lock_count;

        assert((0 <= g_lock_count) && "Must lock server before unlock!");
        return S_OK;
}


//-------------------------------------------------
//                   Functions 
//-------------------------------------------------
void foo()
{
        puts("this is foo");
}

void CALLBACK Rundll32Test(HWND, HINSTANCE, char* cmd_line, int)
{
        MessageBoxA(NULL, cmd_line, "Rundll32 ด๚ธี", MB_OK);
}

void kerker()
{
        puts("kerker");
}
