#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define	__bitwise
#define __devinitdata
#define __init
#define	__initconst
#define	__devinit
#define	__devexit
#define __exit
#define	____cacheline_aligned		__aligned(CACHE_LINE_SIZE)
#define	____cacheline_aligned_in_smp	__aligned(CACHE_LINE_SIZE)

#if __has_attribute(__counted_by__)
#define	__counted_by(_x)		__attribute__((__counted_by__(_x)))
#else
#define	__counted_by(_x)
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define	__counted_by_le(_x)		__counted_by(_x)
#define	__counted_by_be(_x)
#else
#define	__counted_by_le(_x)
#define	__counted_by_be(_x)		__counted_by(_x)
#endif

#define	likely(x)			__builtin_expect(!!(x), 1)
#define	unlikely(x)			__builtin_expect(!!(x), 0)
#define typeof(x)			__typeof(x)

#define	uninitialized_var(x)		x = x
#define	barrier()			__asm__ __volatile__("": : :"memory")

#define	WRITE_ONCE(x,v) do {		\
	barrier();			\
	(*(volatile __typeof(x) *)(uintptr_t)&(x)) = (v); \
	barrier();			\
} while (0)

#define	READ_ONCE(x) ({			\
	__typeof(x) __var = ({		\
		barrier();		\
		(*(const volatile __typeof(x) *)&(x)); \
	});				\
	barrier();			\
	__var;				\
})

#define	lockless_dereference(p) READ_ONCE(p)

#define	_AT(T,X)	((T)(X))
#define	__must_be_array(a)	__same_type(a, &(a)[0])

#define	sizeof_field(_s, _m)	sizeof(((_s *)0)->_m)

#define is_signed_type(t)	((t)-1 < (t)1)
#define is_unsigned_type(t)	((t)-1 > (t)1)

#if __has_builtin(__builtin_dynamic_object_size)
#define	__struct_size(_s)	__builtin_dynamic_object_size(_s, 0)
#else
#define	__struct_size(_s)	__builtin_object_size(_s, 0)
#endif

#ifdef __cplusplus
}
#endif