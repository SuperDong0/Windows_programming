#ifndef IUNKNOWNIMPL_H
#define IUNKNOWNIMPL_H

#include <Unknwn.h>

class IUnknownImpl : public IUnknown {
public:
        IUnknownImpl();
        virtual ~IUnknownImpl();
        
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

private:
        ULONG ref_count_;
};

#endif  // End of header guard
