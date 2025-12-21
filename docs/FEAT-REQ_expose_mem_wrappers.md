**inject TestLib's wrapped functions into Memory object** at test time! This gives you:

1. **CoreLib uses Memory.alloc()** → goes through injected wrappers
2. **Atomic counters get incremented**
3. **Memory tracking still works**
4. **No LD_PRELOAD needed!**

## Implementation Strategy

### Step 1: Add Function Pointer Setters to Memory Interface

```c
// In memory.h
typedef void* (*alloc_func_t)(size_t);
typedef void (*free_func_t)(void*);
typedef void* (*realloc_func_t)(void*, size_t);
typedef void* (*calloc_func_t)(size_t, size_t);

void Memory_set_allocators(alloc_func_t alloc_fn, 
                          free_func_t free_fn,
                          realloc_func_t realloc_fn,
                          calloc_func_t calloc_fn);
```

### Step 2: Modify memory.c to Use Configurable Allocators

```c
// In memory.c
static alloc_func_t custom_alloc = NULL;
static free_func_t custom_free = NULL;
static realloc_func_t custom_realloc = NULL;
static calloc_func_t custom_calloc = NULL;

static void* memory_alloc(usize size, bool zee) {
    // Use custom allocator if set, otherwise malloc
    alloc_func_t alloc_fn = custom_alloc ? custom_alloc : malloc;
    
    object ptr = alloc_fn(size);
    if (zee && ptr) {
        memset(ptr, 0, size);
    }
    
    // Rest of your tracking logic...
    if (memory_ready && current_page) {
        // ... tracking code
    }
    return ptr;
}

static void memory_dispose(object ptr) {
    free_func_t free_fn = custom_free ? custom_free : free;
    
    if (memory_ready && current_page) {
        // ... untracking code
    }
    free_fn(ptr);
}

void Memory_set_allocators(alloc_func_t alloc_fn, 
                          free_func_t free_fn,
                          realloc_func_t realloc_fn,
                          calloc_func_t calloc_fn) {
    custom_alloc = alloc_fn;
    custom_free = free_fn;
    custom_realloc = realloc_fn;
    custom_calloc = calloc_fn;
}
```

### Step 3: TestLib Exports Its Wrapped Functions

```c
// In sigtest.c (TestLib)
__attribute__((visibility("default")))
void* testlib_wrapped_malloc(size_t size) {
    return __wrap_malloc(size); // Your existing wrapper
}

__attribute__((visibility("default")))
void testlib_wrapped_free(void* ptr) {
    __wrap_free(ptr);
}

// Export getters for atomic counters
__attribute__((visibility("default")))
size_t testlib_get_alloc_count(void) {
    return atomic_load(&global_allocs);
}

__attribute__((visibility("default")))
size_t testlib_get_free_count(void) {
    return atomic_load(&global_frees);
}
```

### Step 4: Test Setup Code Connects Everything

```c
// In your test runner or test setup
#include "sigcore/memory.h"
#include "sigtest.h" // Or link against TestLib

void setup_memory_tracking(void) {
    // Get TestLib's wrapped functions
    void* (*test_malloc)(size_t) = dlsym(RTLD_DEFAULT, "testlib_wrapped_malloc");
    void (*test_free)(void*) = dlsym(RTLD_DEFAULT, "testlib_wrapped_free");
    
    if (test_malloc && test_free) {
        // Inject into Memory object
        Memory_set_allocators(test_malloc, test_free, NULL, NULL);
        
        printf("Memory tracking connected to TestLib wrappers\n");
    } else {
        printf("Warning: TestLib wrappers not found, using standard allocators\n");
    }
}
```

## Why This Works Beautifully

1. **CoreLib already uses Memory.alloc()** (not raw malloc)
2. **Memory.alloc() calls whatever function is set**
3. **In tests, we set it to TestLib's wrapped functions**
4. **All allocations go through TestLib's counters**
5. **Memory object still tracks addresses as before**

## The Flow Becomes:

```
CoreLib code → Memory.alloc() → TestLib's __wrap_malloc → real malloc
                                    ↑
                              Atomic counters increment
                                    ↓
                              Memory.track() called (via hook or directly)
```

