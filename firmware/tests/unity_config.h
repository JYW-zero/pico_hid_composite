#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/* 32-bit host build - explicit widths */
#define UNITY_INT_WIDTH      32
#define UNITY_LONG_WIDTH     32
#define UNITY_POINTER_WIDTH  32

/* Exclude all system headers from Unity internals to avoid glibc parse conflicts */
#define UNITY_EXCLUDE_SETJMP_H
#define UNITY_EXCLUDE_MATH_H
#define UNITY_EXCLUDE_STDDEF_H
#define UNITY_EXCLUDE_STDINT_H
#define UNITY_EXCLUDE_LIMITS_H

/* No noreturn attribute needed for host test build */
#define UNITY_NORETURN

#endif
