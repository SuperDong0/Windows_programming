#ifndef COMPTR_H
#define COMPTR_H


template <typename T>
class ComPtr {
public:
        ~ComPtr() 
        {
                ptr_->Release();
        }

        T** operator&() 
        {
                return &ptr_;
        }

        T* operator->() 
        {
                return ptr_;
        }

        ComPtr<T>& operator=(ComPtr<T>& right)
        {
                ptr_ = right.ptr_;
                ptr_->AddRef();

                return *this;
        }

private:
        T* ptr_;
};


#endif  // End of header guard
