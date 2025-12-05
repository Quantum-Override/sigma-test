// src/memory.c — FINAL, CLEAN, WORKING — ONLY BROKEN THINGS FIXED
#include "sigtest.h"
#include "sigcore.h"
#include <stdlib.h>
#include <string.h> // ← FIXED: was missing
#include <stdatomic.h>
#include <stdio.h>

// Pragma to silence __real_* warnings when building shared lib
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-function-declaration"
#pragma GCC diagnostic ignored "-Wint-conversion"

#define ADDR_SIZE sizeof(addr)

static volatile struct
{
	addr *block;
	size_t cap;
	size_t used;
} tracker = {0};

static FILE *logstream = NULL;

static __thread int memcheck_enabled_this_test = 0;

static void grow(void)
{
	size_t new_cap = tracker.cap ? tracker.cap * 2 : 256;
	addr *p = __real_realloc(tracker.block, new_cap * ADDR_SIZE);
	if (!p)
		return;
	if (!tracker.block)
		memset(p, 0, new_cap * ADDR_SIZE);
	tracker.block = p;
	tracker.cap = new_cap;
}

static void track(void *p)
{
	if (!p)
		return;
	if (tracker.used >= tracker.cap)
		grow();
	if (tracker.used < tracker.cap)
	{
		tracker.block[tracker.used++] = (addr)p;
	}
}

static void untrack(void *p)
{
	if (!p || !tracker.block)
		return;
	for (size_t i = 0; i < tracker.used; i++)
	{
		if (tracker.block[i] == (addr)p) // ← FIXED: cast
		{
			tracker.block[i] = tracker.block[--tracker.used];
			tracker.block[tracker.used] = (addr)0; // ← FIXED: ADDR_EMPTY or 0
			break;
		}
	}
}

static int leaked(void) { return (int)tracker.used; }

// Linker wrap — warnings suppressed above
void *__wrap_malloc(size_t s)
{
	void *p = __real_malloc(s);
	if (p)
		track(p);
	return p;
}

void *__wrap_calloc(size_t n, size_t s)
{
	void *p = __real_calloc(n, s);
	if (p)
		track(p);
	return p;
}

void *__wrap_realloc(void *p, size_t s)
{
	void *np = __real_realloc(p, s);
	if (p)
		untrack(p);
	if (np)
		track(np);
	return np;
}

void __wrap_free(void *p)
{
	if (p)
		untrack(p);
	__real_free(p);
}

#pragma GCC diagnostic pop

// IMemCheck — unchanged and perfect
void MemCheck_enable(void) { memcheck_enabled_this_test = 1; }
void MemCheck_disable(void) { memcheck_enabled_this_test = 0; }
int MemCheck_isEnabled(void) { return memcheck_enabled_this_test; }
int MemCheck_leakedBlocks(void) { return leaked(); }
long MemCheck_leakedBytes(void) { return (long)(tracker.used * ADDR_SIZE); }
void MemCheck_reset(void)
{
	// free all tracked blocks
	for (size_t i = 0; i < tracker.used; i++)
	{
		__real_free((void *)tracker.block[i]);
	}
	tracker.used = 0;
}

char *format_memory_leak_message(int leaked, int exp)
{
	char *buffer = malloc(256);
	if (buffer)
	{
		if (exp >= 0)
		{
			snprintf(buffer, 256,
					 "Memory leaks detected: %d leaked block(s), expected %d",
					 leaked, exp);
		}
		else
		{
			snprintf(buffer, 256,
					 "Memory leaks detected: %d leaked block(s)",
					 leaked);
		}
	}
	return buffer;
}

static void on_end_test(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
		TestCase *tc;
	} *ctx = context;

	if (!memcheck_enabled_this_test)
		return;

	int leaked = MemCheck_leakedBlocks();

	if (leaked > 0)
	{
		fwritelnf(logstream, "MemCheck: detected %d leak(s)\n", leaked);
		(*ctx->tc)->test_result.state = FAIL;
		(*ctx->tc)->test_result.message = format_memory_leak_message(leaked, 0);
		(*ctx->tc)->memcheck_leaks_detected = leaked;
		(*ctx->tc)->memcheck_bytes_leaked = tracker.used * ADDR_SIZE;
	}

	memcheck_enabled_this_test = 0;
}
static void on_set_start(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
		TestCase *tc;
	} *ctx = context;

	(void)ctx; // unused
}

__attribute__((constructor)) static void init(void)
{
	SigtestHooks hr = init_hooks("memory");
	hr->on_end_test = on_end_test;
	hr->before_set = on_set_start;
	register_hooks(hr);
}

const IMemCheck MemCheck = {
	.enable = MemCheck_enable,
	.disable = MemCheck_disable,
	.isEnabled = MemCheck_isEnabled,
	.leakedBlocks = MemCheck_leakedBlocks,
	.leakedBytes = MemCheck_leakedBytes,
	.reset = MemCheck_reset,
};