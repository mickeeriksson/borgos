#ifndef BAREBONE_NEW_H
#define BAREBONE_NEW_H

// Clang kräver en storlekstyp för globala operatorer
typedef __SIZE_TYPE__ size_t;

// Deklaration av standard global new/delete (utan undantag/exceptions)
//void* operator new(size_t size) noexcept;
//void* operator new[](size_t size) noexcept;
void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;

// Deklaration av placement new (viktigt för barebone-system)
inline void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}
inline void* operator new[](size_t, void* ptr) noexcept {
    return ptr;
}

// Tomma deklarationer för delete kopplat till placement new
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}

#endif // BAREBONE_NEW_H