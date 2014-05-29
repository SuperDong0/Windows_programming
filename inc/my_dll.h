#ifndef MY_DLL_H
#define MY_DLL_H

#ifdef BUILDDLL
        #define DLLAPI __declspec(dllexport)
#else
        #define DLLAPI __declspec(dllimport)
#endif

#include <windows.h>
#include <tchar.h>

DLLAPI void foo();
DLLAPI void CALLBACK Rundll32Test(HWND window, HINSTANCE instance, char* cmd_line, int cmd_show);

//-------------------------------------------------
//                COM interface
//-------------------------------------------------
class ISimpleCom : public IUnknown {
public:
        virtual STDMETHODIMP foo(BSTR str) = 0;
};

class ISimpleCom2 : public IUnknown {
public:
        virtual STDMETHODIMP bar(BSTR str) = 0;
};


#endif  // End of header guard
