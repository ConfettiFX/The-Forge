/************************************************************************************

PublicHeader:   OVR_Kernel.h
Filename    :   OVR_BitFlags.h
Content     :   Template for typesafe number types.
Created     :   June 5, 2014
Authors     :   Jonathan E. wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#if !defined(OVR_BitFlags_h)
#define OVR_BitFlags_h

namespace OVR {

//==============================================================
// BitFlagsT
//
// This class provides a wrapper for C-like bit flags allowing standard syntax such as:
//
// flags |= FLAG;	// set a flag
// flags &= ~FLAG;	// clear a flag
//
// while providing type safety and making the flag type obvious.  For instance,
// one might write an interface function such as:
//
//	void DoSomething( const int flags );
//
// Because the flags parameter is just an int, a client using the interface has no way
// of knowing what flags to pass for the second parameter without either being shown
// an example, a comment, or inspecting the code inside of DoSomething(), which may not
// be available to them!
//
// Using an enum instead of an int presents its own problems because as soon as enums
// are combined with the | operator they become an int and casting is required.
//
// Instead, this template class combines the type-safety of enums and (most of) the syntax
// of integer types and allows:
//
//	void DoSomething( const SomeFlagsType flags );
//
// The SomeFlagsType can easily be found and deciphered because it is defined as:
//
//	enum eSomeFlagsType
//	{
//		// NOTE: do not assign actual bit values! The template handles that for simplicity.
//		// The value of the enums needs to be the BIT POSITION and NOT the BIT VALUE!!!
//		SOMEFLAG_ONE,
//		SOMEFLAG_BIT_TWO,
//		SOMEFLAG_BIT_THREE,
//		SOMEFLAG_BIT_FOUR,
//		// etc.
//	};
//
//	typedef BitFlagsT< eFlagsType > SomeFlagsType;
//
// The flags are all exposed in one place and there is a link from the interface
// directly to the type.
//
// There are still some minor syntax differences:
//
// To maintain type safety, construction directly from integer types is not allowed.
// This is invalid:
//
// SomeFlagsType flags( SOMEFLAG_ONE | SOMEFLAG_TWO );
//
// Instead use one of the following expression patterns:
//
// SomeFlagsType flags( SomeFlagsType( SOMEFLAG_ONE ) | SOMEFLAG_TWO );
// SomeFlagsType flags = SomeFlagsType( SOMEFLAG_ONE ) | SOMEFLAG_TWO;
// SomeFlagsType flags( SomeFlagsType() | SOMEFLAG_ONE | SOMEFLAG_TWO );
// SomeFlagsType flags = SomeFlagsType() | SOMEFLAG_ONE | SOMEFLAG_TWO;
//
// NOTE: do not assign actual bit values ( i.e. 1 << 0, 1 << 1, etc.) to the enums.
// The class internally will shift by the value of the enum to get the bit value.
// The value of each enum should be the BIT POSITION and not the BIT VALUE!!!

// pass ALL_BITS to the BitFlagsT constructor to set all bit flags.
enum eAllBits { ALL_BITS };

template <typename _enumType_, typename _storageType_ = int>
class BitFlagsT {
   public:
    static _storageType_ AllBits() {
        size_t numBits = sizeof(_storageType_) * 8;
        _storageType_ topBit = (1ULL << (numBits - 1));
        _storageType_ allButTopBit = topBit - 1ULL;
        return allButTopBit | topBit;
    }

    BitFlagsT() : Value(0) {}

    inline _storageType_ ToBit(const _enumType_ e) {
        return static_cast<_storageType_>(1) << e;
    }

    BitFlagsT(const _enumType_ e) : Value(ToBit(e)) {}

    BitFlagsT(const BitFlagsT& other) : Value(other.Value) {}

    BitFlagsT(const eAllBits allBits) : Value(AllBits()) {}

    BitFlagsT operator|(const BitFlagsT& rhs) const {
        _storageType_ v = Value | rhs.GetValue();
        return BitFlagsT(v);
    }

    bool operator&(const BitFlagsT& rhs) const {
        return (Value & rhs.GetValue()) != 0;
    }

    BitFlagsT& operator=(const BitFlagsT& rhs) {
        if (this != &rhs) {
            this->Value = rhs.Value;
        }
        return *this;
    }

    BitFlagsT& operator|=(const BitFlagsT& rhs) {
        Value |= rhs.GetValue();
        return *this;
    }

    BitFlagsT& operator&=(const BitFlagsT& rhs) {
        Value &= rhs.GetValue();
        return *this;
    }

    BitFlagsT operator~() const {
        return BitFlagsT(~this->GetValue());
    }

    _storageType_ GetValue() const {
        return Value;
    }

   private:
    _storageType_ Value;

   private:
    // It's conceivable we might want to expose this as public, as long as it remains explicit
    // to allow assignment of mixed flags, but that reduces the type safety of this template
    // significantly.
    explicit BitFlagsT(const _storageType_ value) : Value(value) {}
};

} // namespace OVR

#endif // OVR_BitFlags_h
