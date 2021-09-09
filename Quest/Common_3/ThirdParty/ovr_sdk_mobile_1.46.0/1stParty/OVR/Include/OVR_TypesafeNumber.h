/************************************************************************************

PublicHeader:   OVR_Kernel.h
Filename    :   OVR_TypesafeNumber.h
Content     :   Template for typesafe number types.
Created     :   March 2, 2014
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#if !defined(OVR_TypesafeNumber_h)
#define OVR_TypesafeNumber_h

namespace OVR {

//----------------------------------------------
// TypesafeNumberT
//
// This template implements typesafe numbers. An object of this templated type will
// act like a built-in integer or floating point type in all respects except that it
// cannot be mixed with other types.
//
// USAGE:
// The second template parameter is normally an enum type.  The enum type is generally
// an empty type definition or has very few members (perhaps an intial value and a max
// value).  This type uniques the instanced template.
//
// EXAMPLE:
//
//     enum MyUniqueType
//     {
//         MYUNIQUE_INITIAL_VALUE = 0
//     };
//
//     typedef TypesafeNumber< unsigned short, MyUniqueType, MYUNIQUE_INIITIAL_VALUE > uniqueType_t;
//
//     uniqueType_t var1;
//     uniqueType_t var2( 100 );
//     unsigned short foo;
//     var1 = var2;    // this works
//     var1 = foo;        // this will cause a compile error
//     uniqueType_t var3( foo );    // this will work because of the explicit constructor
//
// In general it is better to do this:
//     var1 = uniqueType_t( foo );
//
// Than it is to do this:
//     var1.Set( foo );
//
// This is because the explicit construction is less ambiguous and therefore easier to identify
// at a glance and search for in a large codebase, especially considering that Set() is a common
// function name.
//
//----------------------------------------------
template <typename T, typename UniqueType, UniqueType InitialValue>
class TypesafeNumberT {
   public:
    // constructors
    TypesafeNumberT();
    explicit TypesafeNumberT(T const value);
    TypesafeNumberT(TypesafeNumberT const& value);

    // NOTE: the assignmnet of a Type value is implemented with "= delete"
    // to demonstrate that the whole point of this template is to
    // enforce type safety. Assignment without explicit conversion to the templated
    // type would break type safety.
    T& operator=(T const value) = delete;

    TypesafeNumberT& operator=(TypesafeNumberT const& rhs);

    // comparison operators
    bool operator==(TypesafeNumberT const& rhs) const;
    bool operator!=(TypesafeNumberT const& rhs) const;

    bool operator<(TypesafeNumberT const& rhs) const;
    bool operator>(TypesafeNumberT const& rhs) const;
    bool operator<=(TypesafeNumberT const& rhs) const;
    bool operator>=(TypesafeNumberT const& rhs) const;

    // unary operators
    TypesafeNumberT& operator++(); // prefix
    TypesafeNumberT operator++(int); // postfix

    TypesafeNumberT& operator--(); // prefix
    TypesafeNumberT operator--(int); // postfix

    // compound assignment operators
    TypesafeNumberT& operator+=(TypesafeNumberT const& rhs);
    TypesafeNumberT& operator-=(TypesafeNumberT const& rhs);
    TypesafeNumberT& operator*=(TypesafeNumberT const& rhs);
    TypesafeNumberT& operator/=(TypesafeNumberT const& rhs);
    TypesafeNumberT& operator%=(TypesafeNumberT const& rhs);

    // binary arithmetic operators
    TypesafeNumberT operator+(TypesafeNumberT const& rhs) const;
    TypesafeNumberT operator-(TypesafeNumberT const& rhs) const;
    TypesafeNumberT operator*(TypesafeNumberT const& rhs) const;
    TypesafeNumberT operator/(TypesafeNumberT const& rhs) const;
    TypesafeNumberT operator%(TypesafeNumberT const& rhs) const;

    T Get() const;
    void Set(T const value);

    // for using as a handle
    void Release() {
        Value = InitialValue;
    }

    bool IsValid() const {
        return Value != InitialValue;
    }

   private:
    T Value; // the value itself
};

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>::TypesafeNumberT()
    : Value(static_cast<T>(InitialValue)) {}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>::TypesafeNumberT(T const value)
    : Value(value) {}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>::TypesafeNumberT(TypesafeNumberT const& value) {
    *this = value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator=(TypesafeNumberT const& rhs) {
    if (&rhs != this) {
        this->Value = rhs.Value;
    }
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator==(
    TypesafeNumberT const& rhs) const {
    return this->Value == rhs.Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator!=(
    TypesafeNumberT const& rhs) const {
    return !operator==(rhs);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator<(
    TypesafeNumberT const& rhs) const {
    return this->Value < rhs.Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator>(
    TypesafeNumberT const& rhs) const {
    return this->Value > rhs.Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator<=(
    TypesafeNumberT const& rhs) const {
    return this->Value <= rhs.Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline bool TypesafeNumberT<T, UniqueType, InitialValue>::operator>=(
    TypesafeNumberT const& rhs) const {
    return this->Value >= rhs.Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator++() {
    this->Value++;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator++(int) {
    // postfix
    TypesafeNumberT temp(*this);
    operator++();
    return temp;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator--() {
    this->Value--;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator--(int) {
    // postfix
    TypesafeNumberT temp(*this);
    operator--();
    return temp;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator+=(TypesafeNumberT const& rhs) {
    this->Value = this->Value + rhs.Value;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator-=(TypesafeNumberT const& rhs) {
    this->Value = this->Value - rhs.Value;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator*=(TypesafeNumberT const& rhs) {
    this->Value = this->Value * rhs.Value;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator/=(TypesafeNumberT const& rhs) {
    this->Value = this->Value / rhs.Value;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>&
TypesafeNumberT<T, UniqueType, InitialValue>::operator%=(TypesafeNumberT const& rhs) {
    this->Value = this->Value % rhs.Value;
    return *this;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator+(TypesafeNumberT const& rhs) const {
    return TypesafeNumberT(this->Value + rhs.Value);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator-(TypesafeNumberT const& rhs) const {
    return TypesafeNumberT(this->Value - rhs.Value);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator*(TypesafeNumberT const& rhs) const {
    return TypesafeNumberT(this->Value * rhs.Value);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator/(TypesafeNumberT const& rhs) const {
    return TypesafeNumberT(this->Value / rhs.Value);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline TypesafeNumberT<T, UniqueType, InitialValue>
TypesafeNumberT<T, UniqueType, InitialValue>::operator%(TypesafeNumberT const& rhs) const {
    return TypesafeNumberT(this->Value % rhs.Value);
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline T TypesafeNumberT<T, UniqueType, InitialValue>::Get() const {
    return this->Value;
}

template <typename T, typename UniqueType, UniqueType InitialValue>
inline void TypesafeNumberT<T, UniqueType, InitialValue>::Set(T const value) {
    this->Value = value;
}

} // namespace OVR

#endif // OVR_TypesafeNumber_h
