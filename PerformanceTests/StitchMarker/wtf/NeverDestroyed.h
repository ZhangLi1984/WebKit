/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <type_traits>
#include <utility>
#include <wtf/RefCounted.h>

// NeverDestroyed is a smart-pointer-like class that ensures that the destructor
// for the given object is never called, but doesn't use the heap to allocate it.
// It's useful for static local variables, and can be used like so:
//
// MySharedGlobal& mySharedGlobal()
// {
//   static NeverDestroyed<MySharedGlobal> myGlobal("Hello", 42);
//   return myGlobal;
// }

namespace WTF {

template<typename T> class NeverDestroyed {
    WTF_MAKE_NONCOPYABLE(NeverDestroyed);

public:
    template<typename... Args> NeverDestroyed(Args&&... args)
    {
        MaybeRelax<T>(new (storagePointer()) T(std::forward<Args>(args)...));
    }

    operator T&() { return *storagePointer(); }
    T& get() { return *storagePointer(); }

    operator const T&() const { return *storagePointer(); }
    const T& get() const { return *storagePointer(); }

private:
    using PointerType = typename std::remove_const<T>::type*;

    PointerType storagePointer() const { return const_cast<PointerType>(reinterpret_cast<const T*>(&m_storage)); }

    // FIXME: Investigate whether we should allocate a hunk of virtual memory
    // and hand out chunks of it to NeverDestroyed instead, to reduce fragmentation.
    typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_storage;

    template<typename PtrType, bool ShouldRelax = std::is_base_of<RefCountedBase, PtrType>::value> struct MaybeRelax {
        explicit MaybeRelax(PtrType*) { }
    };
    template<typename PtrType> struct MaybeRelax<PtrType, true> {
        explicit MaybeRelax(PtrType* ptr) { ptr->relaxAdoptionRequirement(); }
    };
};

template<typename T> NeverDestroyed<T> makeNeverDestroyed(T&&);

// FIXME: It's messy to have to repeat the whole class just to make this "lazy" version.
// Should revisit clients to see if we really need this, and perhaps use templates to
// share more of the code with the main NeverDestroyed above.
template<typename T> class LazyNeverDestroyed {
    WTF_MAKE_NONCOPYABLE(LazyNeverDestroyed);

public:
    LazyNeverDestroyed() = default;

    template<typename... Args>
    void construct(Args&&... args)
    {
        ASSERT(!m_isConstructed);

#if !ASSERT_DISABLED
        m_isConstructed = true;
#endif

        MaybeRelax<T>(new (storagePointer()) T(std::forward<Args>(args)...));
    }

    operator T&() { return *storagePointer(); }
    T& get() { return *storagePointer(); }

    T* operator->() { return storagePointer(); }

    operator const T&() const { return *storagePointer(); }
    const T& get() const { return *storagePointer(); }

    const T* operator->() const { return storagePointer(); }

private:
    using PointerType = typename std::remove_const<T>::type*;

    PointerType storagePointer() const
    {
        ASSERT(m_isConstructed);
        return const_cast<PointerType>(reinterpret_cast<const T*>(&m_storage));
    }

    // FIXME: Investigate whether we should allocate a hunk of virtual memory
    // and hand out chunks of it to NeverDestroyed instead, to reduce fragmentation.
    typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_storage;

    template<typename PtrType, bool ShouldRelax = std::is_base_of<RefCountedBase, PtrType>::value> struct MaybeRelax {
        explicit MaybeRelax(PtrType*) { }
    };
    template<typename PtrType> struct MaybeRelax<PtrType, true> {
        explicit MaybeRelax(PtrType* ptr) { ptr->relaxAdoptionRequirement(); }
    };

#if !ASSERT_DISABLED
    // LazyNeverDestroyed objects are always static, so this variable is initialized to false.
    // It must not be initialized dynamically; that would not be thread safe.
    bool m_isConstructed;
#endif
};

template<typename T> inline NeverDestroyed<T> makeNeverDestroyed(T&& argument)
{
    return WTFMove(argument);
}

} // namespace WTF;

using WTF::LazyNeverDestroyed;
using WTF::NeverDestroyed;
using WTF::makeNeverDestroyed;
