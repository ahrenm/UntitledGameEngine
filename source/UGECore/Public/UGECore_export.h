
#ifndef UGECORE_EXPORT_H
#define UGECORE_EXPORT_H

#ifdef UGECORE_STATIC_DEFINE
#  define UGECORE_EXPORT
#  define UGECORE_NO_EXPORT
#else
#  ifndef UGECORE_EXPORT
#    ifdef UGECore_EXPORTS
        /* We are building this library */
#      define UGECORE_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define UGECORE_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef UGECORE_NO_EXPORT
#    define UGECORE_NO_EXPORT 
#  endif
#endif

#ifndef UGECORE_DEPRECATED
#  define UGECORE_DEPRECATED __declspec(deprecated)
#endif

#ifndef UGECORE_DEPRECATED_EXPORT
#  define UGECORE_DEPRECATED_EXPORT UGECORE_EXPORT UGECORE_DEPRECATED
#endif

#ifndef UGECORE_DEPRECATED_NO_EXPORT
#  define UGECORE_DEPRECATED_NO_EXPORT UGECORE_NO_EXPORT UGECORE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef UGECORE_NO_DEPRECATED
#    define UGECORE_NO_DEPRECATED
#  endif
#endif

#endif /* UGECORE_EXPORT_H */
