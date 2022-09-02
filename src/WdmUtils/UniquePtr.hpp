#pragma once
#include <memory>
namespace SPC
{
    template <typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag>
    class SpcDefaultDeleter {
    public:
        constexpr SpcDefaultDeleter() noexcept = default;
        SpcDefaultDeleter(const SpcDefaultDeleter<_Ty>&) noexcept 
        {
            //no copy constructor
        }
        virtual void operator()(_Ty* _Ptr) const noexcept
        {
            static_assert(0 < sizeof(_Ty), "can't delete an incomplete type");
        }
    };

    template <typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag>
    class CppDeleter : SpcDefaultDeleter<_Ty, _PoolType, _PoolTag>
    { 
    public:
        constexpr CppDeleter() noexcept = default;
        CppDeleter(const CppDeleter<_Ty>&) noexcept {}
        void operator()(_Ty* _Ptr) const noexcept 
        {
            SpcDefaultDeleter<_Ty, _PoolType, _PoolTag>::operator(_Ptr);
            delete _Ptr;
        }
    };

    template <typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag>
    class WinKernelDeleter : SpcDefaultDeleter<_Ty, _PoolType, _PoolTag> 
    {
    public:
        static constexpr POOL_TYPE pool_type = _PoolType;
        static constexpr ULONG pool_tag = _PoolTag;
        constexpr KernelDeleter() noexcept = default;

        KernelDeleter(const KernelDeleter<_Ty, _PoolType, _PoolTag>&) noexcept
        {}
        void operator()(_Ty* _Ptr) const noexcept {
            SpcDefaultDeleter<_Ty, _PoolType, _PoolTag>::operator(_Ptr);
            ExFreePoolWithTag(_Ptr, _PoolTag);
        }
    };

    template<typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag, class _Dx = CppDeleter<_Ty, _PoolType, _PoolTag>>
    class CUniquePtr
        {
        public:
            using DataType = _Ty;
            using DeleterType = _Dx;

            CKernalAutoPtr(DataType* ptr = nullptr) noexcept {
                this->Ptr = ptr;
            }

            ~CKernalAutoPtr() noexcept {
                if (nullptr != this->Ptr)
                    this->Deleter(this->Ptr);
            }

            CKernalAutoPtr& operator=(CKernalAutoPtr<_Ty, _PoolType, _PoolTag, _Dx>&& _Right) noexcept 
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

            void Reset(_Ty* new_ptr = nullptr) noexcept {
                _Ty* old_ptr = NULL;
                old_ptr = this->Ptr;
                this->Ptr = new_ptr;

                if (old_ptr)
                {
                    this->Deleter()(old_ptr);
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

            friend class CUniquePtr;
        };

    template<typename _Ty, POOL_TYPE _PoolType, ULONG _PoolTag, class _Dx = WinKernelDeleter<_Ty, _PoolType, _PoolTag>>
        class CWinUniquePtr : CUniquePtr<_Ty, _PoolType, _PoolTag>
        {
        protected:
            friend class CWinUniquePtr;
        };
}
