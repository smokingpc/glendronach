#pragma once
// P0607R0 Inline Variables For The STL
#if _HAS_CXX17
#define _INLINE_VAR inline
#else // _HAS_CXX17
#define _INLINE_VAR
#endif // _HAS_CXX17

namespace SPC
{
    template <typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag>
    class SpcCppDeleter //: SpcDefaultDeleter<_Ty, _PoolType, _PoolTag>
    { 
    public:
        constexpr SpcCppDeleter() noexcept = default;
        SpcCppDeleter(const SpcCppDeleter<_Ty, _PoolType, _PoolTag>&) noexcept {}
        
        void operator()(_Ty* _Ptr) const noexcept 
        {
            //SpcDefaultDeleter<_Ty, _PoolType, _PoolTag>::operator(_Ptr);
            static_assert(0 < sizeof(_Ty), "can't delete an incomplete type");
            delete _Ptr;
        }
    };

    template<typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag, class _Dx = SpcCppDeleter<_Ty, _PoolType, _PoolTag>>
    class CAutoPtr 
    {
    public:
        using DataType = _Ty;
        using DeleterType = _Dx;

        CAutoPtr() noexcept {
            this->Ptr = nullptr;
        }
        CAutoPtr(DataType* ptr) noexcept {
            this->Ptr = ptr;
        }
        CAutoPtr(PVOID ptr) noexcept {
            this->Ptr = (DataType*)ptr;
        }

        virtual ~CAutoPtr() noexcept {
            Reset();
        }

        CAutoPtr& operator=(CAutoPtr<_Ty, _PoolType, _PoolTag, _Dx>&& _Right) noexcept
        {
            Reset(_Right.release());
            return *this;
        }

        operator _Ty* () const
        {
            return this->Ptr;
        }

        _Ty* operator->() const noexcept
        {
            return this->Ptr;
        }

        void Set(_Ty* ptr)  noexcept {
            this->Ptr = ptr;
        }

        _Ty* Get() const noexcept {
            return this->Ptr;
        }
            
        bool IsNull()  const noexcept 
        {
            return (NULL==this->Ptr);
        }

        void Reset(_Ty* new_ptr = nullptr) noexcept {
            _Ty* old_ptr = NULL;
            old_ptr = this->Ptr;
            this->Ptr = new_ptr;

            if (old_ptr)
            {
                this->Deleter(old_ptr);
            }
        }

        _Ty* Release() noexcept {
            _Ty* old_ptr = this->Ptr;
            this->Ptr = nullptr;
            return old_ptr;
        }

    protected:
        DataType* Ptr = nullptr;
        DeleterType Deleter;

        friend class CAutoPtr;
    };
}
