#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef void* fcontext_t;

    struct transfer_t
    {
        fcontext_t ctx;
        void* data;
    };

    struct contextstack_t
    {
        void* sptr;
        size_t ssize;
    };

    /**
     * Callback definition for coroutine
     */
    typedef void (*pfn_coroutine)(transfer_t);

    /**
     * Switches to another context
     * @param to Target context to switch to
     * @param vp Custom user pointer to pass to new context
     */
    transfer_t jump_fcontext(fcontext_t const to, void * vp);

    /**
     * Make a new context
     * @param sp Pointer to allocated stack memory
     * @param size Stack memory size
     * @param 
     */
    fcontext_t make_fcontext(void * sp, size_t size, pfn_coroutine corofn);

    transfer_t ontop_fcontext(fcontext_t const to, void * vp, transfer_t(*fn)(transfer_t));   

    int stack_create(contextstack_t* s, size_t size = 0);
    void stack_destroy(contextstack_t* s);

#ifdef __cplusplus
}
#endif