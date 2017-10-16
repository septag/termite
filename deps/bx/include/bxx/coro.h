#pragma once

/*
 * Copyright (c) 2001-2012 Marc Alexander Lehmann <schmorp@schmorp.de>
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 *
 * This library is modelled strictly after Ralf S. Engelschalls article at
 * http://www.gnu.org/software/pth/rse-pmt.ps. So most of the credit must
 * go to Ralf S. Engelschall <rse@engelschall.com>.
 *
 * This coroutine library is very much stripped down. You should either
 * build your own process abstraction using it or - better - just use GNU
 * Portable Threads, http://www.gnu.org/software/pth/.
 *
 */

/*
 * 2006-10-26 Include stddef.h on OS X to work around one of its bugs.
 *            Reported by Michael_G_Schwern.
 * 2006-11-26 Use _setjmp instead of setjmp on GNU/Linux.
 * 2007-04-27 Set unwind frame info if gcc 3+ and ELF is detected.
 *            Use _setjmp instead of setjmp on _XOPEN_SOURCE >= 600.
 * 2007-05-02 Add assembly versions for x86 and amd64 (to avoid reliance
 *            on SIGUSR2 and sigaltstack in Crossfire).
 * 2008-01-21 Disable CFI usage on anything but GNU/Linux.
 * 2008-03-02 Switched to 2-clause BSD license with GPL exception.
 * 2008-04-04 New (but highly unrecommended) pthreads backend.
 * 2008-04-24 Reinstate CORO_LOSER (had wrong stack adjustments).
 * 2008-10-30 Support assembly method on x86 with and without frame pointer.
 * 2008-11-03 Use a global asm statement for CORO_ASM, idea by pippijn.
 * 2008-11-05 Hopefully fix misaligned stacks with CORO_ASM/SETJMP.
 * 2008-11-07 rbp wasn't saved in CORO_ASM on x86_64.
 *            introduce coro_destroy, which is a nop except for pthreads.
 *            speed up CORO_PTHREAD. Do no longer leak threads either.
 *            coro_create now allows one to create source coro_contexts.
 *            do not rely on makecontext passing a void * correctly.
 *            try harder to get _setjmp/_longjmp.
 *            major code cleanup/restructuring.
 * 2008-11-10 the .cfi hacks are no longer needed.
 * 2008-11-16 work around a freebsd pthread bug.
 * 2008-11-19 define coro_*jmp symbols for easier porting.
 * 2009-06-23 tentative win32-backend support for mingw32 (Yasuhiro Matsumoto).
 * 2010-12-03 tentative support for uclibc (which lacks all sorts of things).
 * 2011-05-30 set initial callee-saved-registers to zero with CORO_ASM.
 *            use .cfi_undefined rip on linux-amd64 for better backtraces.
 * 2011-06-08 maybe properly implement weird windows amd64 calling conventions.
 * 2011-07-03 rely on __GCC_HAVE_DWARF2_CFI_ASM for cfi detection.
 * 2011-08-08 cygwin trashes stacks, use pthreads with double stack on cygwin.
 * 2012-12-04 reduce misprediction penalty for x86/amd64 assembly switcher.
 * 2012-12-05 experimental fiber backend (allocates stack twice).
 * 2012-12-07 API version 3 - add coro_stack_alloc/coro_stack_free.
 * 2012-12-21 valgrind stack registering was broken.
 */

 #if __cplusplus
extern "C" {
#endif

/*
 * This library consists of only three files
 * coro.h, coro.c and LICENSE (and optionally README)
 *
 * It implements what is known as coroutines, in a hopefully
 * portable way.
 *
 * All compiletime symbols must be defined both when including coro.h
 * (using libcoro) as well as when compiling coro.c (the implementation).
 *
 * You can manually specify which flavour you want. If you don't define
 * any of these, libcoro tries to choose a safe and fast default:
 *
 * -DCORO_UCONTEXT
 *
 *    This flavour uses SUSv2's get/set/swap/makecontext functions that
 *    unfortunately only some unices support, and is quite slow.
 *
 * -DCORO_SJLJ
 *
 *    This flavour uses SUSv2's setjmp/longjmp and sigaltstack functions to
 *    do it's job. Coroutine creation is much slower than UCONTEXT, but
 *    context switching is a bit cheaper. It should work on almost all unices.
 *
 * -DCORO_LINUX
 *
 *    CORO_SJLJ variant.
 *    Old GNU/Linux systems (<= glibc-2.1) only work with this implementation
 *    (it is very fast and therefore recommended over other methods, but
 *    doesn't work with anything newer).
 *
 * -DCORO_LOSER
 *
 *    CORO_SJLJ variant.
 *    Microsoft's highly proprietary platform doesn't support sigaltstack, and
 *    this selects a suitable workaround for this platform. It might not work
 *    with your compiler though - it has only been tested with MSVC 6.
 *
 * -DCORO_FIBER
 *
 *    Slower, but probably more portable variant for the Microsoft operating
 *    system, using fibers. Ignores the passed stack and allocates it internally.
 *    Also, due to bugs in cygwin, this does not work with cygwin.
 *
 * -DCORO_IRIX
 *
 *    CORO_SJLJ variant.
 *    For SGI's version of Microsoft's NT ;)
 *
 * -DCORO_ASM
 *
 *    Hand coded assembly, known to work only on a few architectures/ABI:
 *    GCC + x86/IA32 and amd64/x86_64 + GNU/Linux and a few BSDs. Fastest choice,
 *    if it works.
 *
 * -DCORO_PTHREAD
 *
 *    Use the pthread API. You have to provide <pthread.h> and -lpthread.
 *    This is likely the slowest backend, and it also does not support fork(),
 *    so avoid it at all costs.
 *
 * If you define neither of these symbols, coro.h will try to autodetect
 * the best/safest model. To help with the autodetection, you should check
 * (e.g. using autoconf) and define the following symbols: HAVE_UCONTEXT_H
 * / HAVE_SETJMP_H / HAVE_SIGALTSTACK.
 */

/*
 * Changes when the API changes incompatibly.
 * This is ONLY the API version - there is no ABI compatibility between releases.
 *
 * Changes in API version 2:
 * replaced bogus -DCORO_LOOSE with grammatically more correct -DCORO_LOSER
 * Changes in API version 3:
 * introduced stack management (CORO_STACKALLOC)
 */
#define CORO_VERSION 3

#include <stddef.h>

/*
 * This is the type for the initialization function of a new coroutine.
 */
typedef void (*coro_func)(void *);

/*
 * A coroutine state is saved in the following structure. Treat it as an
 * opaque type. errno and sigmask might be saved, but don't rely on it,
 * implement your own switching primitive if you need that.
 */
typedef struct coro_context coro_context;

/*
 * This function creates a new coroutine. Apart from a pointer to an
 * uninitialised coro_context, it expects a pointer to the entry function
 * and the single pointer value that is given to it as argument.
 *
 * Allocating/deallocating the stack is your own responsibility.
 *
 * As a special case, if coro, arg, sptr and ssze are all zero,
 * then an "empty" coro_context will be created that is suitable
 * as an initial source for coro_transfer.
 *
 * This function is not reentrant, but putting a mutex around it
 * will work.
 */
void coro_create (coro_context *ctx, /* an uninitialised coro_context */
                  coro_func coro,    /* the coroutine code to be executed */
                  void *arg,         /* a single pointer passed to the coro */
                  void *sptr,        /* start of stack area */
                  size_t ssze);      /* size of stack area in bytes */

/*
 * The following prototype defines the coroutine switching function. It is
 * sometimes implemented as a macro, so watch out.
 *
 * This function is thread-safe and reentrant.
 */
#if 0
void coro_transfer (coro_context *prev, coro_context *next);
#endif

/*
 * The following prototype defines the coroutine destroy function. It
 * is sometimes implemented as a macro, so watch out. It also serves no
 * purpose unless you want to use the CORO_PTHREAD backend, where it is
 * used to clean up the thread. You are responsible for freeing the stack
 * and the context itself.
 *
 * This function is thread-safe and reentrant.
 */
#if 0
void coro_destroy (coro_context *ctx);
#endif

/*****************************************************************************/
/* optional stack management                                                 */
/*****************************************************************************/
/*
 * You can disable all of the stack management functions by
 * defining CORO_STACKALLOC to 0. Otherwise, they are enabled by default.
 *
 * If stack management is enabled, you can influence the implementation via these
 * symbols:
 *
 * -DCORO_USE_VALGRIND
 *
 *    If defined, then libcoro will include valgrind/valgrind.h and register
 *    and unregister stacks with valgrind.
 *
 * -DCORO_GUARDPAGES=n
 *
 *    libcoro will try to use the specified number of guard pages to protect against
 *    stack overflow. If n is 0, then the feature will be disabled. If it isn't
 *    defined, then libcoro will choose a suitable default. If guardpages are not
 *    supported on the platform, then the feature will be silently disabled.
 */
#ifndef CORO_STACKALLOC
# define CORO_STACKALLOC 1
#endif

#if CORO_STACKALLOC

/*
 * The only allowed operations on these struct members is to read the
 * "sptr" and "ssze" members to pass it to coro_create, to read the "sptr"
 * member to see if it is false, in which case the stack isn't allocated,
 * and to set the "sptr" member to 0, to indicate to coro_stack_free to
 * not actually do anything.
 */

struct coro_stack
{
  void *sptr;
  size_t ssze;
#if CORO_USE_VALGRIND
  int valgrind_id;
#endif
};

/*
 * Try to allocate a stack of at least the given size and return true if
 * successful, or false otherwise.
 *
 * The size is *NOT* specified in bytes, but in units of sizeof (void *),
 * i.e. the stack is typically 4(8) times larger on 32 bit(64 bit) platforms
 * then the size passed in.
 *
 * If size is 0, then a "suitable" stack size is chosen (usually 1-2MB).
 */
int coro_stack_alloc (struct coro_stack *stack, unsigned int size);

/*
 * Free the stack allocated by coro_stack_alloc again. It is safe to
 * call this function on the coro_stack structure even if coro_stack_alloc
 * failed.
 */
void coro_stack_free (struct coro_stack *stack);

#endif

/*
 * That was it. No other user-serviceable parts below here.
 */

/*****************************************************************************/

#if !defined CORO_LOSER      && !defined CORO_UCONTEXT \
    && !defined CORO_SJLJ    && !defined CORO_LINUX \
    && !defined CORO_IRIX    && !defined CORO_ASM \
    && !defined CORO_PTHREAD && !defined CORO_FIBER
# if defined WINDOWS && (defined __i386 || (__x86_64 || defined _M_IX86 || defined _M_AMD64))
#  define CORO_ASM 1
# elif defined WINDOWS || defined _WIN32
#  define CORO_LOSER 1 /* you don't win with windoze */
# elif __linux && (__i386 || (__x86_64 && !__ILP32))
#  define CORO_ASM 1
# elif defined HAVE_UCONTEXT_H
#  define CORO_UCONTEXT 1
# elif defined HAVE_SETJMP_H && defined HAVE_SIGALTSTACK
#  define CORO_SJLJ 1
# else
error unknown or unsupported architecture
# endif
#endif

/*****************************************************************************/

#if CORO_UCONTEXT

# include <ucontext.h>

struct coro_context
{
  ucontext_t uc;
};

# define coro_transfer(p,n) swapcontext (&((p)->uc), &((n)->uc))
# define coro_destroy(ctx) (void *)(ctx)

#elif CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX

# if defined(CORO_LINUX) && !defined(_GNU_SOURCE)
#  define _GNU_SOURCE /* for glibc */
# endif

# if !CORO_LOSER
#  include <unistd.h>
# endif

/* solaris is hopelessly borked, it expands _XOPEN_UNIX to nothing */
# if __sun
#  undef _XOPEN_UNIX
#  define _XOPEN_UNIX 1
# endif

# include <setjmp.h>

# if _XOPEN_UNIX > 0 || defined (_setjmp)
#  define coro_jmp_buf      jmp_buf
#  define coro_setjmp(env)  _setjmp (env)
#  define coro_longjmp(env) _longjmp ((env), 1)
# elif CORO_LOSER
#  define coro_jmp_buf      jmp_buf
#  define coro_setjmp(env)  setjmp (env)
#  define coro_longjmp(env) longjmp ((env), 1)
# else
#  define coro_jmp_buf      sigjmp_buf
#  define coro_setjmp(env)  sigsetjmp (env, 0)
#  define coro_longjmp(env) siglongjmp ((env), 1)
# endif

struct coro_context
{
  coro_jmp_buf env;
};

# define coro_transfer(p,n) do { if (!coro_setjmp ((p)->env)) coro_longjmp ((n)->env); } while (0)
# define coro_destroy(ctx) (void *)(ctx)

#elif CORO_ASM

struct coro_context
{
  void **sp; /* must be at offset 0 */
};

void __attribute__ ((__noinline__, __regparm__(2)))
coro_transfer (coro_context *prev, coro_context *next);

# define coro_destroy(ctx) (void *)(ctx)

#elif CORO_PTHREAD

# include <pthread.h>

extern pthread_mutex_t coro_mutex;

struct coro_context
{
  pthread_cond_t cv;
  pthread_t id;
};

void coro_transfer (coro_context *prev, coro_context *next);
void coro_destroy (coro_context *ctx);

#elif CORO_FIBER

struct coro_context
{
  void *fiber;
  /* only used for initialisation */
  coro_func coro;
  void *arg;
};

void coro_transfer (coro_context *prev, coro_context *next);
void coro_destroy (coro_context *ctx);

#endif

#if __cplusplus
}
#endif

#ifdef BX_IMPLEMENT_CORO

#include <stddef.h>
#include <string.h>

/*****************************************************************************/
/* ucontext/setjmp/asm backends                                              */
/*****************************************************************************/
#if CORO_UCONTEXT || CORO_SJLJ || CORO_LOSER || CORO_LINUX || CORO_IRIX || CORO_ASM

# if CORO_UCONTEXT
#  include <stddef.h>
# endif

# if !defined(STACK_ADJUST_PTR)
#  if __sgi
/* IRIX is decidedly NON-unix */
#   define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss) - 8)
#   define STACK_ADJUST_SIZE(sp,ss) ((ss) - 8)
#  elif (__i386__ && CORO_LINUX) || (_M_IX86 && CORO_LOSER)
#   define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss))
#   define STACK_ADJUST_SIZE(sp,ss) (ss)
#  elif (__amd64__ && CORO_LINUX) || ((_M_AMD64 || _M_IA64) && CORO_LOSER)
#   define STACK_ADJUST_PTR(sp,ss) ((char *)(sp) + (ss) - 8)
#   define STACK_ADJUST_SIZE(sp,ss) (ss)
#  else
#   define STACK_ADJUST_PTR(sp,ss) (sp)
#   define STACK_ADJUST_SIZE(sp,ss) (ss)
#  endif
# endif

# include <stdlib.h>

# if CORO_SJLJ
#  include <stdio.h>
#  include <signal.h>
#  include <unistd.h>
# endif

static coro_func coro_init_func;
static void *coro_init_arg;
static coro_context *new_coro, *create_coro;

static void
coro_init (void)
{
  volatile coro_func func = coro_init_func;
  volatile void *arg = coro_init_arg;

  coro_transfer (new_coro, create_coro);

#if __GCC_HAVE_DWARF2_CFI_ASM && __amd64
  asm (".cfi_undefined rip");
#endif

  func ((void *)arg);

  /* the new coro returned. bad. just abort() for now */
  abort ();
}

# if CORO_SJLJ

static volatile int trampoline_done;

/* trampoline signal handler */
static void
trampoline (int sig)
{
  if (coro_setjmp (new_coro->env))
    coro_init (); /* start it */
  else
    trampoline_done = 1;
}

# endif

# if CORO_ASM

  #if _WIN32 || __CYGWIN__
    #define CORO_WIN_TIB 1
  #endif

  asm (
       "\t.text\n"
       #if _WIN32 || __CYGWIN__
       "\t.globl _coro_transfer\n"
       "_coro_transfer:\n"
       #else
       "\t.globl coro_transfer\n"
       "coro_transfer:\n"
       #endif
       /* windows, of course, gives a shit on the amd64 ABI and uses different registers */
       /* http://blogs.msdn.com/freik/archive/2005/03/17/398200.aspx */
       #if __amd64

         #if _WIN32 || __CYGWIN__
           #define NUM_SAVED 29
           "\tsubq $168, %rsp\t" /* one dummy qword to improve alignment */
           "\tmovaps %xmm6, (%rsp)\n"
           "\tmovaps %xmm7, 16(%rsp)\n"
           "\tmovaps %xmm8, 32(%rsp)\n"
           "\tmovaps %xmm9, 48(%rsp)\n"
           "\tmovaps %xmm10, 64(%rsp)\n"
           "\tmovaps %xmm11, 80(%rsp)\n"
           "\tmovaps %xmm12, 96(%rsp)\n"
           "\tmovaps %xmm13, 112(%rsp)\n"
           "\tmovaps %xmm14, 128(%rsp)\n"
           "\tmovaps %xmm15, 144(%rsp)\n"
           "\tpushq %rsi\n"
           "\tpushq %rdi\n"
           "\tpushq %rbp\n"
           "\tpushq %rbx\n"
           "\tpushq %r12\n"
           "\tpushq %r13\n"
           "\tpushq %r14\n"
           "\tpushq %r15\n"
           #if CORO_WIN_TIB
             "\tpushq %fs:0x0\n"
             "\tpushq %fs:0x8\n"
             "\tpushq %fs:0xc\n"
           #endif
           "\tmovq %rsp, (%rcx)\n"
           "\tmovq (%rdx), %rsp\n"
           #if CORO_WIN_TIB
             "\tpopq %fs:0xc\n"
             "\tpopq %fs:0x8\n"
             "\tpopq %fs:0x0\n"
           #endif
           "\tpopq %r15\n"
           "\tpopq %r14\n"
           "\tpopq %r13\n"
           "\tpopq %r12\n"
           "\tpopq %rbx\n"
           "\tpopq %rbp\n"
           "\tpopq %rdi\n"
           "\tpopq %rsi\n"
           "\tmovaps (%rsp), %xmm6\n"
           "\tmovaps 16(%rsp), %xmm7\n"
           "\tmovaps 32(%rsp), %xmm8\n"
           "\tmovaps 48(%rsp), %xmm9\n"
           "\tmovaps 64(%rsp), %xmm10\n"
           "\tmovaps 80(%rsp), %xmm11\n"
           "\tmovaps 96(%rsp), %xmm12\n"
           "\tmovaps 112(%rsp), %xmm13\n"
           "\tmovaps 128(%rsp), %xmm14\n"
           "\tmovaps 144(%rsp), %xmm15\n"
           "\taddq $168, %rsp\n"
         #else
           #define NUM_SAVED 6
           "\tpushq %rbp\n"
           "\tpushq %rbx\n"
           "\tpushq %r12\n"
           "\tpushq %r13\n"
           "\tpushq %r14\n"
           "\tpushq %r15\n"
           "\tmovq %rsp, (%rdi)\n"
           "\tmovq (%rsi), %rsp\n"
           "\tpopq %r15\n"
           "\tpopq %r14\n"
           "\tpopq %r13\n"
           "\tpopq %r12\n"
           "\tpopq %rbx\n"
           "\tpopq %rbp\n"
         #endif
         "\tpopq %rcx\n"
         "\tjmpq *%rcx\n"

       #elif __i386

         #define NUM_SAVED 4
         "\tpushl %ebp\n"
         "\tpushl %ebx\n"
         "\tpushl %esi\n"
         "\tpushl %edi\n"
         #if CORO_WIN_TIB
           #undef NUM_SAVED
           #define NUM_SAVED 7
           "\tpushl %fs:0\n"
           "\tpushl %fs:4\n"
           "\tpushl %fs:8\n"
         #endif
         "\tmovl %esp, (%eax)\n"
         "\tmovl (%edx), %esp\n"
         #if CORO_WIN_TIB
           "\tpopl %fs:8\n"
           "\tpopl %fs:4\n"
           "\tpopl %fs:0\n"
         #endif
         "\tpopl %edi\n"
         "\tpopl %esi\n"
         "\tpopl %ebx\n"
         "\tpopl %ebp\n"
         "\tpopl %ecx\n"
         "\tjmpl *%ecx\n"

       #else
         #error unsupported architecture
       #endif
  );

# endif

void
coro_create (coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
  coro_context nctx;
# if CORO_SJLJ
  stack_t ostk, nstk;
  struct sigaction osa, nsa;
  sigset_t nsig, osig;
# endif

  if (!coro)
    return;

  coro_init_func = coro;
  coro_init_arg  = arg;

  new_coro    = ctx;
  create_coro = &nctx;

# if CORO_SJLJ
  /* we use SIGUSR2. first block it, then fiddle with it. */

  sigemptyset (&nsig);
  sigaddset (&nsig, SIGUSR2);
  sigprocmask (SIG_BLOCK, &nsig, &osig);

  nsa.sa_handler = trampoline;
  sigemptyset (&nsa.sa_mask);
  nsa.sa_flags = SA_ONSTACK;

  if (sigaction (SIGUSR2, &nsa, &osa))
    {
      perror ("sigaction");
      abort ();
    }

  /* set the new stack */
  nstk.ss_sp    = STACK_ADJUST_PTR (sptr, ssize); /* yes, some platforms (IRIX) get this wrong. */
  nstk.ss_size  = STACK_ADJUST_SIZE (sptr, ssize);
  nstk.ss_flags = 0;

  if (sigaltstack (&nstk, &ostk) < 0)
    {
      perror ("sigaltstack");
      abort ();
    }

  trampoline_done = 0;
  kill (getpid (), SIGUSR2);
  sigfillset (&nsig); sigdelset (&nsig, SIGUSR2);

  while (!trampoline_done)
    sigsuspend (&nsig);

  sigaltstack (0, &nstk);
  nstk.ss_flags = SS_DISABLE;
  if (sigaltstack (&nstk, 0) < 0)
    perror ("sigaltstack");

  sigaltstack (0, &nstk);
  if (~nstk.ss_flags & SS_DISABLE)
    abort ();

  if (~ostk.ss_flags & SS_DISABLE)
    sigaltstack (&ostk, 0);

  sigaction (SIGUSR2, &osa, 0);
  sigprocmask (SIG_SETMASK, &osig, 0);

# elif CORO_LOSER

  coro_setjmp (ctx->env);
  #if __CYGWIN__ && __i386
    ctx->env[8]                        = (long)    coro_init;
    ctx->env[7]                        = (long)    ((char *)sptr + ssize)         - sizeof (long);
  #elif __CYGWIN__ && __x86_64
    ctx->env[7]                        = (long)    coro_init;
    ctx->env[6]                        = (long)    ((char *)sptr + ssize)         - sizeof (long);
  #elif defined __MINGW32__
    ctx->env[5]                        = (long)    coro_init;
    ctx->env[4]                        = (long)    ((char *)sptr + ssize)         - sizeof (long);
  #elif defined _M_IX86
    ((_JUMP_BUFFER *)&ctx->env)->Eip   = (long)    coro_init;
    ((_JUMP_BUFFER *)&ctx->env)->Esp   = (long)    STACK_ADJUST_PTR (sptr, ssize) - sizeof (long);
  #elif defined _M_AMD64
    ((_JUMP_BUFFER *)&ctx->env)->Rip   = (__int64) coro_init;
    ((_JUMP_BUFFER *)&ctx->env)->Rsp   = (__int64) STACK_ADJUST_PTR (sptr, ssize) - sizeof (__int64);
  #elif defined _M_IA64
    ((_JUMP_BUFFER *)&ctx->env)->StIIP = (__int64) coro_init;
    ((_JUMP_BUFFER *)&ctx->env)->IntSp = (__int64) STACK_ADJUST_PTR (sptr, ssize) - sizeof (__int64);
  #else
    #error "microsoft libc or architecture not supported"
  #endif

# elif CORO_LINUX

  coro_setjmp (ctx->env);
  #if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined (JB_PC) && defined (JB_SP)
    ctx->env[0].__jmpbuf[JB_PC]        = (long)    coro_init;
    ctx->env[0].__jmpbuf[JB_SP]        = (long)    STACK_ADJUST_PTR (sptr, ssize) - sizeof (long);
  #elif __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined (__mc68000__)
    ctx->env[0].__jmpbuf[0].__aregs[0] = (long int)coro_init;
    ctx->env[0].__jmpbuf[0].__sp       = (int *)   ((char *)sptr + ssize)         - sizeof (long);
  #elif defined (__GNU_LIBRARY__) && defined (__i386__)
    ctx->env[0].__jmpbuf[0].__pc       = (char *)  coro_init;
    ctx->env[0].__jmpbuf[0].__sp       = (void *)  ((char *)sptr + ssize)         - sizeof (long);
  #elif defined (__GNU_LIBRARY__) && defined (__amd64__)
    ctx->env[0].__jmpbuf[JB_PC]        = (long)    coro_init;
    ctx->env[0].__jmpbuf[0].__sp       = (void *)  ((char *)sptr + ssize)         - sizeof (long);
  #else
    #error "linux libc or architecture not supported"
  #endif

# elif CORO_IRIX

  coro_setjmp (ctx->env, 0);
  ctx->env[JB_PC]                      = (__uint64_t)coro_init;
  ctx->env[JB_SP]                      = (__uint64_t)STACK_ADJUST_PTR (sptr, ssize) - sizeof (long);

# elif CORO_ASM

  ctx->sp = (void **)(ssize + (char *)sptr);
  *--ctx->sp = (void *)abort; /* needed for alignment only */
  *--ctx->sp = (void *)coro_init;

  #if CORO_WIN_TIB
  *--ctx->sp = 0;                    /* ExceptionList */
  *--ctx->sp = (char *)sptr + ssize; /* StackBase */
  *--ctx->sp = sptr;                 /* StackLimit */
  #endif

  ctx->sp -= NUM_SAVED;
  bx::memSet (ctx->sp, 0, sizeof (*ctx->sp) * NUM_SAVED);

# elif CORO_UCONTEXT

  getcontext (&(ctx->uc));

  ctx->uc.uc_link           =  0;
  ctx->uc.uc_stack.ss_sp    = sptr;
  ctx->uc.uc_stack.ss_size  = (size_t)ssize;
  ctx->uc.uc_stack.ss_flags = 0;

  makecontext (&(ctx->uc), (void (*)())coro_init, 0);

# endif

  coro_transfer (create_coro, new_coro);
}

/*****************************************************************************/
/* pthread backend                                                           */
/*****************************************************************************/
#elif CORO_PTHREAD

/* this mutex will be locked by the running coroutine */
pthread_mutex_t coro_mutex = PTHREAD_MUTEX_INITIALIZER;

struct coro_init_args
{
  coro_func func;
  void *arg;
  coro_context *self, *main;
};

static pthread_t null_tid;

/* I'd so love to cast pthread_mutex_unlock to void (*)(void *)... */
static void
mutex_unlock_wrapper (void *arg)
{
  pthread_mutex_unlock ((pthread_mutex_t *)arg);
}

static void *
coro_init (void *args_)
{
  struct coro_init_args *args = (struct coro_init_args *)args_;
  coro_func func = args->func;
  void *arg = args->arg;

  pthread_mutex_lock (&coro_mutex);

  /* we try to be good citizens and use deferred cancellation and cleanup handlers */
  pthread_cleanup_push (mutex_unlock_wrapper, &coro_mutex);
    coro_transfer (args->self, args->main);
    func (arg);
  pthread_cleanup_pop (1);

  return 0;
}

void
coro_transfer (coro_context *prev, coro_context *next)
{
  pthread_cond_signal (&next->cv);
  pthread_cond_wait (&prev->cv, &coro_mutex);
#if __FreeBSD__ /* freebsd is of course broken and needs manual testcancel calls... yay... */
  pthread_testcancel ();
#endif
}

void
coro_create (coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
  static coro_context nctx;
  static int once;

  if (!once)
    {
      once = 1;

      pthread_mutex_lock (&coro_mutex);
      pthread_cond_init (&nctx.cv, 0);
      null_tid = pthread_self ();
    }

  pthread_cond_init (&ctx->cv, 0);

  if (coro)
    {
      pthread_attr_t attr;
      struct coro_init_args args;

      args.func = coro;
      args.arg  = arg;
      args.self = ctx;
      args.main = &nctx;

      pthread_attr_init (&attr);
#if __UCLIBC__
      /* exists, but is borked */
      /*pthread_attr_setstacksize (&attr, (size_t)ssize);*/
#elif __CYGWIN__
      /* POSIX, not here */
      pthread_attr_setstacksize (&attr, (size_t)ssize);
#else
      pthread_attr_setstack (&attr, sptr, (size_t)ssize);
#endif
      pthread_attr_setscope (&attr, PTHREAD_SCOPE_PROCESS);
      pthread_create (&ctx->id, &attr, coro_init, &args);

      coro_transfer (args.main, args.self);
    }
  else
    ctx->id = null_tid;
}

void
coro_destroy (coro_context *ctx)
{
  if (!pthread_equal (ctx->id, null_tid))
    {
      pthread_cancel (ctx->id);
      pthread_mutex_unlock (&coro_mutex);
      pthread_join (ctx->id, 0);
      pthread_mutex_lock (&coro_mutex);
    }

  pthread_cond_destroy (&ctx->cv);
}

/*****************************************************************************/
/* fiber backend                                                             */
/*****************************************************************************/
#elif CORO_FIBER

#define WIN32_LEAN_AND_MEAN
#if _WIN32_WINNT < 0x0400
  #undef _WIN32_WINNT
  #define _WIN32_WINNT 0x0400
#endif
#include <windows.h>

VOID CALLBACK
coro_init (PVOID arg)
{
  coro_context *ctx = (coro_context *)arg;

  ctx->coro (ctx->arg);
}

void
coro_transfer (coro_context *prev, coro_context *next)
{
  if (!prev->fiber)
    {
      prev->fiber = GetCurrentFiber ();

      if (prev->fiber == 0 || prev->fiber == (void *)0x1e00)
        prev->fiber = ConvertThreadToFiber (0);
    }

  SwitchToFiber (next->fiber);
}

void
coro_create (coro_context *ctx, coro_func coro, void *arg, void *sptr, size_t ssize)
{
  ctx->fiber = 0;
  ctx->coro  = coro;
  ctx->arg   = arg;

  if (!coro)
    return;

  ctx->fiber = CreateFiber (ssize, coro_init, ctx);
}

void
coro_destroy (coro_context *ctx)
{
  DeleteFiber (ctx->fiber);
}

#else
  #error unsupported backend
#endif

/*****************************************************************************/
/* stack management                                                          */
/*****************************************************************************/
#if CORO_STACKALLOC

#include <stdlib.h>

#ifndef _WIN32
# include <unistd.h>
#endif

#if CORO_USE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#if _POSIX_MAPPED_FILES
# include <sys/mman.h>
# define CORO_MMAP 1
# ifndef MAP_ANONYMOUS
#  ifdef MAP_ANON
#   define MAP_ANONYMOUS MAP_ANON
#  else
#   undef CORO_MMAP
#  endif
# endif
# include <limits.h>
#else
# undef CORO_MMAP
#endif

#if _POSIX_MEMORY_PROTECTION
# ifndef CORO_GUARDPAGES
#  define CORO_GUARDPAGES 4
# endif
#else
# undef CORO_GUARDPAGES
#endif

#if !CORO_MMAP
# undef CORO_GUARDPAGES
#endif

#if !__i386 && !__x86_64 && !__powerpc && !__m68k && !__alpha && !__mips && !__sparc64
# undef CORO_GUARDPAGES
#endif

#ifndef CORO_GUARDPAGES
# define CORO_GUARDPAGES 0
#endif

#if !PAGESIZE
  #if !CORO_MMAP
    #define PAGESIZE 4096
  #else
    static size_t
    coro_pagesize (void)
    {
      static size_t pagesize;

      if (!pagesize)
        pagesize = sysconf (_SC_PAGESIZE);

      return pagesize;
    }

    #define PAGESIZE coro_pagesize ()
  #endif
#endif

int
coro_stack_alloc (struct coro_stack *stack, unsigned int size)
{
  if (!size)
    size = 256 * 1024;

  stack->sptr = 0;
  stack->ssze = ((size_t)size * sizeof (void *) + PAGESIZE - 1) / PAGESIZE * PAGESIZE;

#if CORO_FIBER

  stack->sptr = (void *)stack;
  return 1;

#else

  size_t ssze = stack->ssze + CORO_GUARDPAGES * PAGESIZE;
  void *base;

  #if CORO_MMAP
    /* mmap supposedly does allocate-on-write for us */
    base = mmap (0, ssze, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (base == (void *)-1)
      {
        /* some systems don't let us have executable heap */
        /* we assume they won't need executable stack in that case */
        base = mmap (0, ssze, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (base == (void *)-1)
          return 0;
      }

    #if CORO_GUARDPAGES
      mprotect (base, CORO_GUARDPAGES * PAGESIZE, PROT_NONE);
    #endif

    base = (void*)((char *)base + CORO_GUARDPAGES * PAGESIZE);
  #else
    base = malloc (ssze);
    if (!base)
      return 0;
  #endif

  #if CORO_USE_VALGRIND
    stack->valgrind_id = VALGRIND_STACK_REGISTER ((char *)base, ((char *)base) + ssze - CORO_GUARDPAGES * PAGESIZE);
  #endif

  stack->sptr = base;
  return 1;

#endif
}

void
coro_stack_free (struct coro_stack *stack)
{
#if CORO_FIBER
  /* nop */
#else
  #if CORO_USE_VALGRIND
    VALGRIND_STACK_DEREGISTER (stack->valgrind_id);
  #endif

  #if CORO_MMAP
    if (stack->sptr)
      munmap ((void*)((char *)stack->sptr - CORO_GUARDPAGES * PAGESIZE),
              stack->ssze                 + CORO_GUARDPAGES * PAGESIZE);
  #else
    free (stack->sptr);
  #endif
#endif
}

#endif

#endif  // BX_IMPLEMENT_CORO
