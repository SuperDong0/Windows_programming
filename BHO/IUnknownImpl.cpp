// #define NDEBUG

#include "IUnknownImpl.h"
// #include "com_helper.h"

IUnknownImpl::IUnknownImpl() : ref_count_(1)
{
}

IUnknownImpl::~IUnknownImpl()
{
}

STDMETHODIMP_(ULONG) IUnknownImpl::AddRef()
{
//        LOG("AddRef count=%u\n", ref_count_ + 1);
        return ++ref_count_;
}

STDMETHODIMP_(ULONG) IUnknownImpl::Release()
{
        ULONG res = --ref_count_;
        if (0 == res)
                delete this;
        
//        LOG("Release count=%u\n", res);
        return res;
}
