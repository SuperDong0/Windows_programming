#include "ComPtr.h"
#include <cstdio>
#include <Unknwn.h>

class ComPtrTest {
public:
        ComPtrTest() : count_(1) {}
        
        ~ComPtrTest()
        {
                printf("Func=%s\n", __FUNCTION__);
        }

        STDMETHODIMP_(ULONG) AddRef()
        {
                printf("Func=%s\n", __FUNCTION__);

                return ++count_;
        }

        STDMETHODIMP_(ULONG) Release()
        {
                printf("Func=%s\n", __FUNCTION__);
                int res = --count_;

                if (0 == res)
                        delete this;

                return res;
        }

        STDMETHODIMP foo()
        {
                printf("Func=%s\n", __FUNCTION__);
                return S_OK;
        }

private:
        int count_;
};

// This function do nothing, it is just used for test the ComPtr template class
void TestHeader() 
{
        ComPtrTest** argument;
        ComPtr<ComPtrTest> com_ptr;
        ComPtr<ComPtrTest> com_ptr2;
        
        argument = &com_ptr;
        *argument = new ComPtrTest();

        com_ptr->foo();
        com_ptr2 = com_ptr;
        com_ptr2->foo();
}
