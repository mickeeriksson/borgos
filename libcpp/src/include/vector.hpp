#ifndef _LIBCPP_VECTOR_HPP
#define _LIBCPP_VECTOR_HPP

#include "types.h"

template <typename T>
class Vector {
private:
    T* m_arr;        // Pointer to dynamically allocated array
    size_t m_size;   // Number of elements in the vector
    size_t m_capacity; // Capacity of the vector

    void resize(); // Utility function to handle resizing

public:
    Vector();                 // Default constructor
    ~Vector();                // Destructor

    Vector(const Vector& other);        // Copy constructor
    Vector& operator=(const Vector& other); // Copy assignment operator

    Vector(Vector&& other) noexcept;        // Move constructor
    Vector& operator=(Vector&& other) noexcept; // Move assignment operator

    void push_back(const T& element); // Add element
    void pop_back(); // Remove last element

    size_t size() const;
    size_t capacity() const;

    T& at(size_t index);
    T& operator[](size_t index);
};

#endif