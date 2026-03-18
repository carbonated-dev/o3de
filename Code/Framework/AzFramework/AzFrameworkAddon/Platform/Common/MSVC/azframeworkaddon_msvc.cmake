# aefimov: we need it no more, but we can override some compilation keys here if we have to

#set(LY_COMPILE_OPTIONS PRIVATE 
#   /W2
#   /WX-
#   /wd5233
#   /DCARBONATED
#   /DENABLE_CRY_PHYSICS
#   /D_ENABLE_EXTENDED_ALIGNED_STORAGE
#   /Zc:preprocessor
#   /DWIN64
#   $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:/Zi>
#   $<$<OR:$<CONFIG:Debug>,$<CONFIG:Profile>>:/DAZ_ENABLE_TRACING>
#   $<$<OR:$<CONFIG:Release>,$<CONFIG:Profile>>:/DNDEBUG>
#)
