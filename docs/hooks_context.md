# Hook Contexts — attaching custom data to hooks

This short example shows how to attach a custom context structure to your `ST_Hooks` so your hook callbacks can access per-run data.

- Place any per-run or per-set state in a custom struct and allocate it before registering hooks.
- Assign the pointer to `hooks->context`.
- Cast `context` back to your struct type inside each hook callback.
- Free/cleanup the context in `after_set` (or another suitable lifecycle hook).

Example

```c
#include <stdlib.h>
#include "sigtest.h"

/* Your custom context for hooks */
typedef struct my_hook_ctx_s {
    int counter;
    FILE *logfile;
} my_hook_ctx_t;

/* A before_test hook that uses the custom context */
static void my_before_test(object context) {
    my_hook_ctx_t *ctx = (my_hook_ctx_t *)context;
    ctx->counter++;
    if (ctx->logfile) fprintf(ctx->logfile, "  - before_test called, counter=%d\n", ctx->counter);
}

/* A on_test_result hook that logs the test outcome */
static void my_on_test_result(const TestSet set, const TestCase tc, object context) {
    my_hook_ctx_t *ctx = (my_hook_ctx_t *)context;
    if (!set || !tc) return;
    if (ctx->logfile) fprintf(ctx->logfile, "  - result for %s: %s\n", tc->name, TEST_STATES[tc->test_result.state]);
}

/* Cleanup the custom context in after_set */
static void my_after_set(TestSet set, object context) {
    my_hook_ctx_t *ctx = (my_hook_ctx_t *)context;
    if (ctx) {
        if (ctx->logfile) fclose(ctx->logfile);
        free(ctx);
        /* Note: set->hooks->context must not be used after this point */
        if (set && set->hooks) set->hooks->context = NULL;
    }
}

/* Register hooks with context */
void register_my_hooks(void) {
    ST_Hooks hooks = init_hooks("my_hooks");
    if (!hooks) return;

    my_hook_ctx_t *ctx = malloc(sizeof(*ctx));
    if (!ctx) return;
    ctx->counter = 0;
    ctx->logfile = fopen("logs/myhooks.log", "w");

    hooks->before_test = my_before_test;
    hooks->on_test_result = my_on_test_result;
    hooks->after_set = my_after_set;
    hooks->context = ctx;

    register_hooks(hooks);
}
```

Notes

- The runner's default hooks use an anonymous context layout internally; if you supply your own `hooks->context` and you also want to use the runner's `default_*` hook functions, your context must be layout-compatible with the defaults. In practice, implement your own callbacks (as shown) and keep `hooks->context` fully under your control.
- Clean up any resources you allocate for the context in a hook (for example `after_set`) or via your test set `cleanup` function.

See also: [src/sigtest.c](src/sigtest.c) for examples of the runner's default hooks and how the runner passes `hooks->context` into callbacks.
