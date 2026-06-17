#pragma once

/**
TODO: Implement spinlocks
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int dummy;
} spinlock_t;

#define spin_lock_init(lock) do { (void)(lock); } while(0)
#define spin_lock_irqsave(lock, flags) do { (void)(lock); (void)(flags); } while(0)
#define spin_unlock_irqrestore(lock, flags) do { (void)(lock); (void)(flags); } while(0)

#ifdef __cplusplus
}
#endif