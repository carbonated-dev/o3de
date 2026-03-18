# aefimov: we need it no more, but we can override some compilation keys here if we have to

#set(LY_COMPILE_OPTIONS PRIVATE 
#   -DCARBONATED
#   -Wno-deprecated-declarations
#   -fexceptions
#)

#set(LY_COMPILE_DEFINITIONS PRIVATE ${LY_COMPILE_DEFINITIONS} 
#   $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:NDEBUG>
#   $<$<CONFIG:Debug>:_DEBUG>
#)

#if(${PAL_PLATFORM_NAME} STREQUAL "iOS")
#   set(LY_COMPILE_DEFINITIONS PRIVATE ${LY_COMPILE_DEFINITIONS} APPLE IOS)
#endif()

#if(${PAL_PLATFORM_NAME} STREQUAL "Mac")
#   set(LY_COMPILE_DEFINITIONS PRIVATE ${LY_COMPILE_DEFINITIONS} APPLE MAC)
#endif()

#if(${PAL_PLATFORM_NAME} STREQUAL "Android")
#    set(LY_COMPILE_DEFINITIONS PRIVATE ${LY_COMPILE_DEFINITIONS}
#            LINUX64
#            _LINUX
#            LINUX
#            ANDROID
#            MOBILE
#            _HAS_C9X
#            ENABLE_TYPE_INFO
#    )
#    set(LY_COMPILE_OPTIONS PRIVATE ${LY_COMPILE_OPTIONS}
#           -Wno-unused-variable
#    )
#endif()
