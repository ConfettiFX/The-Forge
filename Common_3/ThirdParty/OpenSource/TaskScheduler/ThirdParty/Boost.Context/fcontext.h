/*
Copyright (c) 2016 Richard Maxwell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef FCONTEXT_H
#define FCONTEXT_H

#include <stdint.h>
// intptr_t

#include <stddef.h>
// size_t

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
// -----------------------------------------------------------------------------

typedef void* fcontext_t;

intptr_t jump_fcontext
(
      fcontext_t* ofc
    , fcontext_t  nfc
    , intptr_t    vp
    , int         preserve_fpu
);

fcontext_t make_fcontext
(
      void*  sp
    , size_t size
    , void  (*fn)(intptr_t)
);
// sp is the pointer to the _top_ of the stack (ie &stack_buffer[size]).

// -----------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
// -----------------------------------------------------------------------------

#endif
