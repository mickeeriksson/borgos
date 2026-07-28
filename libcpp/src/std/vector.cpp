#include <vector.hpp>
#include "error.h"
//#include "log.h"

template <typename T>
Vector<T>::Vector() : m_size(0), m_capacity(1) {
    m_arr = new T[m_capacity];
    //log_msg("######### Construct Vector\n");
}

template <typename T>
Vector<T>::~Vector() {
    delete[] m_arr;
    //log_msg("######### Delete Vector\n");

}

//Copy constructor
template <typename T>
Vector<T>::Vector(const Vector& other) : m_size(other.m_size), m_capacity(other.m_capacity) {
    m_arr = new T[m_capacity];
    for (size_t i = 0; i < m_size; ++i) {
        m_arr[i] = other.m_arr[i];
    }
}

//Copy assigment constructor
template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& other) {
    if (this == &other)
        return *this; // Self-assignment check

    delete[] m_arr; // Free existing memory

    m_size = other.m_size;
    m_capacity = other.m_capacity;
    m_arr = new T[m_capacity];
    for (size_t i = 0; i < m_size; ++i) {
        m_arr[i] = other.m_arr[i];
    }

    return *this;
}

//Move constructor
template <typename T>
Vector<T>::Vector(Vector&& other) noexcept
    : m_arr(other.m_arr), m_size(other.m_size), m_capacity(other.m_capacity) {
    other.m_arr = nullptr; // Nullify the other vector's pointer
    other.m_size = 0;
    other.m_capacity = 0;
}


//Move assignment constructor
template <typename T>
Vector<T>& Vector<T>::operator=(Vector&& other) noexcept {
    if (this == &other)
        return *this; // Self-assignment check

    delete[] m_arr; // Release the current memory

    m_arr = other.m_arr; // Take ownership of the other vector's data
    m_size = other.m_size;
    m_capacity = other.m_capacity;

    other.m_arr = nullptr; // Reset the other vector
    other.m_size = 0;
    other.m_capacity = 0;

    return *this;
}

//resize
template <typename T>
void Vector<T>::resize() {
    m_capacity *= 2;
    T* new_arr = new T[m_capacity];
    for (size_t i = 0; i < m_size; ++i) {
        new_arr[i] = m_arr[i];
    }
    delete[] m_arr;
    m_arr = new_arr;
}


//push_back
//If the current size equals capacity, we resize the array before adding the new element.
template <typename T>
void Vector<T>::push_back(const T& element) {
    if (m_size == m_capacity) {
        resize(); // Resize if we reach capacity
    }
    m_arr[m_size] = element;
    ++m_size;
}

template <typename T>
void Vector<T>::pop_back() {
    if (m_size > 0) {
        --m_size;
    }
}

template <typename T>
size_t Vector<T>::size() const {
    return m_size;
}

template <typename T>
size_t Vector<T>::capacity() const {
    return m_capacity;
}

template <typename T>
T& Vector<T>::operator[](size_t index) {
    if (index >= m_size) {
        PANIC("Index out of bounds");
        //throw std::out_of_range("Index out of bounds");
    }
    return m_arr[index];
}

template <typename T>
T& Vector<T>::at(size_t index) {
    if (index >= m_size) {
        PANIC("Index out of bounds");
        //throw std::out_of_range("Index out of bounds");
    }
    return m_arr[index];
}


template class Vector<int>;
template class Vector<void*>;
