/*
This file is added to resolve incomatibilities between o3de and ly in GridMate module.
Remove this file when porting is completed.
*/
#ifndef GRID_MATE_GRUNBER_PATCHES_H
#define GRID_MATE_GRUNBER_PATCHES_H 1


// Gruber patch begin // VMED
    // Source: lloyd-atom\dev\Code\Framework\AzCore\AzCore\std\typetraits\conditional.h
    // Backwards comaptible AZStd extensions for std::conditional and std::enable_if
namespace AZStd::Utils
{
    // Use AZStd::conditional_t instead
    template<bool Condition, typename T, typename F>
    using if_c = AZStd::conditional<Condition, T, F>;
    // Use AZStd::enable_if_t instead
    template<bool Condition, typename T = void>
    using enable_if_c = AZStd::enable_if<Condition, T>;
}
// Gruber patch end // VMED

// Gruber patch begin // VMED
    // Source: lloyd-atom\dev\Code\Framework\AzCore\AzCore\RTTI\TypeInfo.h
    // Fall-back for the original version of AZ_TYPE_INFO that accepted template arguments. This should not be used, unless
    // to fix issues where AZ_TYPE_INFO was incorrectly used and the old UUID has to be maintained.
    //#define AZ_TYPE_INFO_LEGACY AZ_TYPE_INFO
#define AZ_TYPE_INFO_LEGACY(...)
// Gruber end // VMED

// Gruber patch begin // VMED
    // removed from o3de
#define AZ_PROFILE_TIMER(...)
// Gruber end // VMED

#endif
