
src_lib_libenesim_la_SOURCES += \
src/lib/argb/libargb_main.c \
src/lib/argb/libargb.h \
src/lib/argb/libargb_argb8888.h \
src/lib/argb/libargb_argb8888_blend.h \
src/lib/argb/libargb_argb8888_core.h \
src/lib/argb/libargb_argb8888_fill.h \
src/lib/argb/libargb_argb8888_misc.h \
src/lib/argb/libargb_argb8888_mul4_sym.h \
src/lib/argb/libargb_argb8888_unpre.h \
src/lib/argb/libargb_macros.h \
src/lib/argb/libargb_mmx.h \
src/lib/argb/libargb_sse2.h \
src/lib/argb/libargb_types.h

if BUILD_ORC
ORC_SOURCES = \
src/lib/argb/libargb_argb8888_blend_orc.c \
src/lib/argb/libargb_argb8888_blend_orc_private.h

BUILT_SOURCES += $(ORC_SOURCES)
src_lib_libenesim_la_SOURCES += $(ORC_SOURCES)

endif
