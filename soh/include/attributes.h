#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#if !defined(__GNUC__) && !defined(__attribute__)
#define __attribute__(x)
#endif

#define UNUSED      __attribute__((unused))
#define FALLTHROUGH __attribute__((fallthrough))
#define NORETURN    __attribute__((noreturn))

#if defined(_WIN32) && defined(UNBOUND_MOD)
#define HOST_DATA __declspec(dllimport)
#else
#define HOST_DATA
#endif

#endif
