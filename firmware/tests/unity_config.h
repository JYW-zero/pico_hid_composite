#ifndef UNITY_CONFIG_H
#define UNITY_CONFIG_H

/* 32-bit host build */
#define UNITY_INT_WIDTH      32
#define UNITY_LONG_WIDTH     32
#define UNITY_POINTER_WIDTH  32

/* No setjmp/longjmp for host test build */
#define UNITY_EXCLUDE_SETJMP_H

/* Avoid stdnoreturn.h include conflicts */
#define UNITY_NORETURN

#endif
