#ifndef ENTT_ENTITY_ENTT_TRAITS_HPP
#define ENTT_ENTITY_ENTT_TRAITS_HPP


#include "../../../../OS/Interfaces/IOperatingSystem.h"


namespace entt {


/**
 * @brief Entity traits.
 *
 * Primary template isn't defined on purpose. All the specializations give a
 * compile-time error unless the template parameter is an accepted entity type.
 */
template<typename>
struct entt_traits;


/**
 * @brief Entity traits for a 16 bits entity identifier.
 *
 * A 16 bits entity identifier guarantees:
 *
 * * 12 bits for the entity number (up to 4k entities).
 * * 4 bit for the version (resets in [0-15]).
 */
template<>
struct entt_traits<uint16> {
    /*! @brief Underlying entity type. */
    using entity_type = uint16;
    /*! @brief Underlying version type. */
    using version_type = uint8;
    /*! @brief Difference type. */
    using difference_type = int32;

    /*! @brief Mask to use to get the entity number out of an identifier. */
    static constexpr uint16 entity_mask = 0xFFF;
    /*! @brief Mask to use to get the version out of an identifier. */
    static constexpr uint16 version_mask = 0xF;
    /*! @brief Extent of the entity number within an identifier. */
    static constexpr auto entity_shift = 12;
};


/**
 * @brief Entity traits for a 32 bits entity identifier.
 *
 * A 32 bits entity identifier guarantees:
 *
 * * 20 bits for the entity number (suitable for almost all the games).
 * * 12 bit for the version (resets in [0-4095]).
 */
template<>
struct entt_traits<uint32> {
    /*! @brief Underlying entity type. */
    using entity_type = uint32;
    /*! @brief Underlying version type. */
    using version_type = uint16;
    /*! @brief Difference type. */
    using difference_type = int64;

    /*! @brief Mask to use to get the entity number out of an identifier. */
    static constexpr uint32 entity_mask = 0xFFFFF;
    /*! @brief Mask to use to get the version out of an identifier. */
    static constexpr uint32 version_mask = 0xFFF;
    /*! @brief Extent of the entity number within an identifier. */
    static constexpr auto entity_shift = 20;
};


/**
 * @brief Entity traits for a 64 bits entity identifier.
 *
 * A 64 bits entity identifier guarantees:
 *
 * * 32 bits for the entity number (an indecently large number).
 * * 32 bit for the version (an indecently large number).
 */
template<>
struct entt_traits<uint64> {
    /*! @brief Underlying entity type. */
    using entity_type = uint64;
    /*! @brief Underlying version type. */
    using version_type = uint32;
    /*! @brief Difference type. */
    using difference_type = int64;

    /*! @brief Mask to use to get the entity number out of an identifier. */
    static constexpr uint64 entity_mask = 0xFFFFFFFF;
    /*! @brief Mask to use to get the version out of an identifier. */
    static constexpr uint64 version_mask = 0xFFFFFFFF;
    /*! @brief Extent of the entity number within an identifier. */
    static constexpr auto entity_shift = 32;
};


}


#endif // ENTT_ENTITY_ENTT_TRAITS_HPP
