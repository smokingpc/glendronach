#pragma once
// ================================================================
// SpcNvme : OpenSource NVMe Driver for Windows 8+
// Author : Roy Wang(SmokingPC).
// Licensed by MIT License.
// 
// Copyright (C) 2022, Roy Wang (SmokingPC)
// https://github.com/smokingpc/
// 
// NVMe Spec: https://nvmexpress.org/specifications/
// Contact Me : smokingpc@gmail.com
// ================================================================
// Permission is hereby granted, free of charge, to any person obtaining a 
// copy of this softwareand associated documentation files(the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and /or sell copies of the Software, and to permit persons to whom the 
// Software is furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS 
// IN THE SOFTWARE.
// ================================================================
// [Additional Statement]
// This Driver is implemented by NVMe Spec 1.3 and Windows Storport Miniport Driver.
// You can copy, modify, redistribute the source code. 
// 
// There is only one requirement to use this source code:
// PLEASE DO NOT remove or modify the "original author" of this codes.
// Each copy or modify in source code should keep all "original author" line in codes.
// 
// Enjoy it.
// ================================================================


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
