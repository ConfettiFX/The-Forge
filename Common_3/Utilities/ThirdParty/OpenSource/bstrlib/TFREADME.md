# Intro

## Example usage
```c
void foo(const char* name)
{
  // You can initialize bstring buffer on stack or on heap

  // 1. Stack initialization
  // no need to initialize strBuf when using bemptyfromarr
  unsigned char strBuf[256];
  // str data will point to strBuf
  bstring str = bemptyfromarr(strBuf);
  
  // 2. Heap initialization
  bstring str = bdynallocfromliteral("", 256);


  // Do some useful work with string
  // safe sprintf that will alloc new buffer on heap if needed
  bformat(&str, "hello %s", name);
  // ...


  // There are 2 ways to finish working with bstring
  // 1. Use bdestroy that will free underlying buffer only if ownage bit was set 
  // (bit is set automatically when bstrlib allocates memory)
  bdestroy(&str);
  // 2. Assert that bstring didn't allocate additional memory
  // Use this approach with caution and only if
  // string was initialized with stack allocated buffer  
  // and you are 100% sure that string should never overflow
  ASSERT(!bownsdata(&str));
}
```

## The Forge changes to bstrlib

bstrlib was refactored a lot in order to satisfy requirements of The Forge

#### Interface changes
+ Added stack allocated buffers support. Most significant bit of `mlen` field is used to signify ownership of underlying string buffer
+ Initialization functions were refactored to never allocate `bstring` struct on heap
+ Many function calls are replaced with macros in order to be able to properly pass memory tracking information. Without it it would be very hard to track where memory leak is happening as it would point to bstrlib functions
  + Memory tracing parameters are passed only when `ENABLE_MEMORY_TRACKING` macro is defined
+ Most of the functions were returning `BSTR_ERR` value on any validation err. We replaced most of the checks with `ASSERT`. But there are still some use cases:
  + Some search functions require error code (e.g. `binstr`)
  + For testing purposes(when `AUTOMATED_TESTING` macro is defined)

#### Implementation changes
+ Most of the functions were refactored to use`<fnName>blk` functions in order to reduce amount of code and required coverage for testing
+ We found some issues with aliasing pointers and fixed those
  + Most `<fnName>cstr` functions still don't handle aliasing pointers because they work optimistically (this reduces amount of `strlen` calls) 
+ When temporary buffer is needed we try to use stack allocated buffers for performance reasons

## TODO

Check implementation and enable tests for following functions:
+ `bsplit`
+ `bsplits`
+ `bsplitstr`
+ `bjoin`
+ `bjoinblk`
+ `bsplitcb`
+ `bsplitscb`
+ `bsplitstrcb`


# Details

## `bstring` struct

```c
struct bstring {
  int mlen;
  int slen;
  unsigned char * data;
};
```

+ `mlen` - capacity of the data buffer
  + We chose to use only 31 bits of `mlen` field. The most significant bit is used to signify ownership of underlying string buffer
  + Use `bmlen` macro to get actual capacity
  + Use `bownsdata` macro to check if underlying buffer is owned by string and will be freed on destruction of string
+ `slen` - length of the actual string
+ `data` - pointer to string buffer

All `const bstring* str`(acts as string_view) passed to bstrlib functions should meet following requirements:
+ `str != NULL`
+ `str->slen >= 0`
+ `str->data != NULL`
> Use `bconstisvalid(const bstring*)` macro to check if constraints above are satisfied

All `bstring* str` passed to bstrlib functions should meet following requirements:
+ `str->mlen != 0`
+ `bmlen(str) >= str->slen`
+ All requirements of `const bstring* str`
> Use `bisvalid(bstring*)` macro to check if constraints above are saisfied

`bstring`s without null terminator are allowed. But all bstrlib functions will append null terminator when possible

## Initialization

All initializations happens in following form:
`bstring str = macroname(args...);`
For constant `bstring`s you can manually set `slen` value and data pointer, set `mlen` to 0 in order to prevent writes into string.

#### Empty string initialization
+ `bempty()`
  + Initializes internal buffer to global empty string
  > Changing values of the underlying buffer to anything besides `\0` can lead to undefined behavior

#### Constant string initialization
All macros from this section initialize `mlen` field to 0.
+ `bconstfromstr(bstring*)` 
  + Copies `slen` and `data` pointer fields of another bstring
+ `bconstfromliteral("some string literal")` 
  + Initializes string from string literal
+ `bconstfromcstr(const char*)` 
  + Initializes string from null-terminated c string
+ `bconstfromblk(void* blk, int size)` 
  + Initializes string from some block of memory

#### Stack allocated strings

+ `bfromarr(unsigned char arr[SIZE])` 
  + Initializes string buffer to point to the given array. The buffer is not modified and length of the string is set to `strlen(arr)`
+ `bemptyfromarr(unsigned char arr[SIZE])`
  + Initializes string buffer to point to the given array. `\0` is written into the first element of the array and length of the string is set to 0

When `BSTR_ENABLE_STATIC_TO_DYNAMIC_CONVERSION` macro is defined the library will allocate new buffer if initial one overflows

#### Heap allocated strings

+ `bdynfromcstr(const char* str)`
  + Creates string allocated on the heap if provided string is not empty. If provided string is empty underlying buffer will point to global shared empty string buffer (same as `bempty`)
+ `bdynallocfromcstr(const char* str, int minCapacity)`
  + Creates string allocated on the heap if provided string is not empty or `minCapacity > 1`. If provided string is empty and `minCapacity <= 1` underlying buffer will point to global shared empty string buffer (same as `bempty`)
+ `bdynfromblk(const void* blk, int len, int minCapacity)`
  + Creates string allocated on the heap if `len > 0` or `minCapacity > 1`. If `len == 0` and `minCapacity <= 1` underlying buffer will point to global shared empty string buffer(same as `bempty`)
+ `bdynfromstr(const bstring* str, int minCapacity)`
  + Creates string allocated on the heap if `str->slen > 0` or `minCapacity > 1`. If `str->slen == 0` and `minCapacity <= 1` underlying buffer will point to global shared empty string buffer(same as `bempty`)

## Allocation functions
+ `bdestroy(bstring* str)`
  + Frees underlying string buffer if ownership bit is set
  + Sets string to `bempty`
+ `balloc(bstring* str, int len)`
  + Increases the size of the memory backing the bstring `str` to at least `len`
  + For small lengths the capacity will be set to powers of 2
+ `ballocmin(bstring* str, int len)`
  + Set the size of the memory backing the bstring `str` to `len` or `str->slen+1`,  whichever is larger.  Note that repeated use of this function can degrade  performance.
  + If `str` is not owned tries to convert to allocate new block only if capacity is not sufficient (important for empty and strings without null terminator)
+ `bmakedynamic(bstring* str, int len)`
  + Same as `balloc` but ensures that string is allocated on heap and ownership bit is set
  + Doesn't allocate any memory if ownership flag was already set
+ `bmakedynamicmin(bstring* str, int len)`
  + Same as `ballocmin` but ensures that string is allocated on heap and ownership bit is set
  + Doesn't allocate any memory if ownership flag was already set
+ `bmakecstr(bstring* str)`
  + Ensures that underlying string buffer is null-terminated

## Assignment functions

+ `bassign(bstring* a, const bstring* b)`
  + Overwrite the string `a` with the contents of string `b`.
+ `bassignmidstr(bstring* a, const bstring* b, int left, int len)`
  + Overwrite the string a with the middle of contents of string b starting from position left and running for a length len.
+ `bassigncstr(bstring* a, const char* b)`
  + Overwrite the string `a` with the contents of `const char* b`
+ `bassignblk(bstring* a, const void* b, int len)`
  + Overwrite the string `a` with the contents of the block `(b, len)`
+ `bassignliteral(bstring* a, "some string literal")`
  + Overwrite the string `a` with the contents of the string literal

## Concatenation
+ `bconcat(bstring* b0, const bstring* b1)`
  + Concatenate the string `b1` to the string `b0`.
+ `bconchar(bstring* b0, char c)`
  + Concatenate char `c` to the string `b0`
+ `bcatliteral(bstring* b, "some string literal")`
  + Concatenate string literal to the string `b`
+ `bcatcstr(bstring* b, const char* s)`
  + Concatenate a char* string to a bstring
+ `bcatblk(bstring* b, const void* s, int len)`
  + Concatenate a fixed length buffer to a bstring

## Insertion
+ `binsert(bstring* b0, int pos, const bstring* b1, unsigned char fill)`
  + Inserts the string `b1` into `b0` at position `pos`.  If the position `pos` is past the end of `b0`, then the character `fill` is appended as necessary to make up the gap between the end of `b0` and `pos`
+ `binsertliteral(bstring* b0, int pos, "some string literal", unsigned char fill)`
  + Inserts the string literal into `b0` at position `pos`.  If the position `pos` is past the end of `b0`, then the character `fill` is appended as necessary to make up the gap between the end of `b` and `pos`
+ `binsertblk(bstring* b, int pos, const void* blk, int len, unsigned char fill)`
  + Inserts the block of characters from `blk` with length `len` into `b` at position `pos`. If the position `pos` is past the end of `b`, then the character `fill` is appended as necessary to make up the gap between the end of `b` and `pos`
+ `binsertch(bstring* b, int pos, int len, unsigned char fill)`
  + Inserts the character `fill` repeatedly into `b` at position `pos` for a length `len`. If the position `pos` is past the end of `b`, then the character `fill` is appended as necessary to make up the gap between the end of `b` and the position `pos + len`


## Replacement
+ `breplace(bstring* b0, int pos, int len,const bstring* b1, unsigned char fill)`
  + Replace a section of a string `b0` from `pos` for a length `len` with the string `b1`. `fill` is used when `pos > b0->slen`.
  + This function is a more efficient version of this code:
    ```c
    if (b0->slen > pos)
      bdelete(b0, pos, len);
    binsert(b0, pos, b1, fill);
    ```
+ `bsetstr(bstring* b0, int pos, const bstring* b1, unsigned char fill)`
  Overwrite the string `b0` starting at position `pos` with the string `b1`. If the position `pos` is past the end of `b0`, then the character `fill` is appended as necessary to make up the gap between the end of `b0` and `pos`. If `b1` is `NULL`, function behaves as if `b1` was a 0-length string.
+ `bfindreplace(bstring* b, const bstring* find, const bstring* repl, int pos)`
  + Replace all occurrences of a `find` string with a `replace` string after `pos` in a `b` bstring. 
+ `bfindreplacecaseless(bstring* b, const bstring* find, const bstring* repl, int pos)`
  + Replace all occurrences of a `find` string, ignoring case, with a `replace`
 string after `pos` in a `b` bstring.
+ `btoupper(bstring* b)`
  + Converts all characters from string `b` to upper case
+ `btolower(bstring* b)`
  + Converts all characters from string `b` to lower case

## Deletion

+ `bdelete(bstring* s1, int pos, int len)`
  + Removes characters from `pos` to `pos+len-1` inclusive and shifts the tail of the `s1` starting from `pos+len` to `pos`. 
  + `len` must be positive for this call to have any effect
+ `btrunc(bstring* b, int n)`
  + Truncate the bstring `b` to at most `n` characters
+ `bltrimws (bstring* b)`
  + Removes whitespace characters from the beginning of the string `b`
+ `brtrimws (bstring* b)`
  + Removes whitespace characters from the end of the string `b`
+ `btrimws (bstring* b)`
  + Removes whitespace characters from the beginning and the end of string `b`

## Comparison and Search


#### `stdlib`-like comparison
Return 0 if equal
Prefer to use `biseq<fnName>` functions to check for equality
+ `int bstrcmp(const bstring* b0, const bstring* b1)`
  + Check the string `b0` and `b1` for equality. A value less than or greater than zero, indicating that the string pointed to by `b0` is lexicographically less than or greater than the string pointed to by `b1` is returned.  If the the string lengths are unequal but the characters up until the length of the shorter are equal then a value less than, or greater than zero, indicating that the string pointed to by b0 is shorter or longer than the string pointed to by b1 is returned.  0 is returned if and only if the two strings are the same.  If the length of the strings are different, this function is O(n).  Like its standard C library counter part `strcmp`, the comparison does not proceed past any `\0` termination characters encountered.
+ `int bstrncmp(const bstring* b0, const bstring* b1, int n)`
  + Compare the string `b0` and `b1` for at most `n` characters.  A value is returned as if `b0` and `b1` were first truncated to at most `n` characters then bstrcmp was called with these new strings are paremeters.  If the length of the strings are different, this function is O(n).  Like its standard C library counterpart `strcmp`, the comparison does not proceed past any `\0` termination characters encountered.
+ `int bstricmp(const bstring* b0, const bstring* b1)`
  + Compare two strings without differentiating between case. The return value is the difference of the values of the characters where the two strings first differ after lower case transformation, otherwise `0` is returned indicating that the strings are equal. If the lengths are different, then a difference from `0` is given, but if the first extra character is `\0`, then `BSTR_CMP_EXTRA_NULL` is returned.
+ `int bstrnicmp(const bstring* b0, const bstring* b1, int n)`
  + Compare two strings without differentiating between case for at most `n` characters. If the position where the two strings first differ is before the `n`th position, the return value is the difference of the values of the characters, otherwise `0` is returned.  If the lengths are different and less than `n` characters, then a difference from `0` is given, but if the first extra character is `\0`,  then `BSTR_CMP_EXTRA_NULL` is returned.


#### Equality comparison
Return 1 if equal (More efficient than `stdlib` functions for checking equality)

+ `int biseq(const bstring* b0, const bstring* b1)`
  + Check the string `b0` and `b1` for equality.  If the strings differ, `0` is returned, if the strings are the same, `1` is returned. If the length of the strings are different, this function is O(1).  `\0` termination characters are not treated in any special way.
+ `int biseqliteral(const bstring* b, "some string literal")`
  + Check the string `b0` and string literal for equality.  If the strings differ, `0` is returned, if the strings are the same, `1` is returned. If the length of the strings are different, this function is O(1). Size of literal string is considered to be equal to `sizeof(literal) - 1`. `\0` termination characters are not treated in any special way
+ `int biseqblk(const bstring* b, const void * blk, int len)`
  + Check the string `b` with the character block `blk` of length `len` for equality. If the content differs, `0` is returned, if the content is the same, `1` is returned.  If the length of the strings are different, this function is O(1). `\0` characters are not treated in any special way.
+ `int bisstemeqliteral(const bstring *b, "some string literal")`
  + Check beginning of string `b` with a string literal for equality. If the beginning of `b` differs from the literal (or if `b` is too short), `0` is returned, if the strings are the same, `1` is returned. Size of literal string is considered to be equal to `sizeof(literal) - 1`. `\0` characters are not treated in any special way.
+ `int bisstemeqblk(const bstring* b, const void * blk, int len)`
  + Check beginning of string `b` with a block of memory `blk` of length `len` for equality. If the beginning of `b` differs from the memory block (or if `b` is too short), `0` is returned, if the strings are the same, `1` is returned.  `\0` characters are not treated in any special way.
+ `int biseqcstr(const bstring* b, const char * s)`
  + Check the bstring `b` and char * string `s` for equality.  The C string `s` must be `\0` terminated at exactly the length of the bstring `b`, and the contents between the two must be identical with the bstring `b` with no `\0` characters for the two contents to be considered equal. This is equivalent to the condition that their current contents will be always be equal when comparing them in the same format after converting one or the other. If the strings are equal `1` is returned, if they are unequal `0` is returned.
+ `int biseqcaseless(const bstring* b0, const bstring* b1)`
  + Check two strings for equality without differentiating between case. If the strings differ other than in case, `0` is returned, if the strings are the same, `1` is returned.  If the length of the strings are different, this function is O(1). `\0` termination characters are not treated in any special way.
+ `int biseqcaselessliteral(const bstring *b, "some string literal")`
  + Check content of `b` with a string literal for equality without differentiating between character case.  If the content differs other than in case, `0` is returned, if, ignoring case, the content is the same, `1` is returned. If the length of the strings are different, this function is O(1). Size of literal string is considered to be equal to `sizeof(literal) - 1`. `\0` characters are not treated in any special way
+ `int biseqcaselessblk(const bstring* b, const void * blk, int len);`
  + Check content of `b` and the array of bytes in `blk` for length `len` for equality without differentiating between character case.  If the content differs other than in case, `0` is returned, if, ignoring case, the content is the same, `1` is returned.  If the length of the strings are different, this function is O(1). `\0` characters are not treated in any special way
+ `int bisstemeqcaselessliteral(const bstring *b, "some string literal")`
  + Check beginning of string `b` with a string literal for equality without differentiating between character case. If the beginning of `b` differs from the literal other than in case (or if `b` is too short), `0` is returned, if the strings are the same, `1` is returned. Size of literal string is considered to be equal to `sizeof(literal) - 1`. `\0` characters are not treated in any special way.
+ `int bisstemeqcaselessblk(const bstring* b, const void * blk, int len)`
  + Check beginning of string `b` with a block of memory of length `len` without differentiating between case for equality.  If the beginning of `b` differs from the memory block other than in case (or if `b` is too short), `0` is returned, if the strings are the same, `1` is returned. `\0` characters are not treated in any special way.
+ `int biseqcstrcaseless(const bstring* b, const char * s)`
  + check the bstring `b` and char * string `s` for equality.  The C string `s` must be `\0` terminated at exactly the length of the bstring `b`, and the contents between the two must be identical except for case with the bstring `b` with no `\0` characters for the two contents to be considered equal.  This is equivalent to the condition that their current contents will be always be equal ignoring case when comparing them in the same format after converting one or the other.  If the strings are equal, except for case, `1` is returned, if they are unequal regardless of case `0` is returned.

### Substring search
+ `int binstr(const bstring* s1, int pos, const bstring* s2)`
  + Search for the bstring `b2` in `b1` starting from position `pos`, and searching forward.  If it is found then return with the first position where it is found, otherwise return `BSTR_ERR`.  Note that this is just a brute force string searcher that does not attempt clever things like the Boyer-Moore search algorithm.  Because of this there are many degenerate cases where this can take much longer than it needs to.
+ `int binstrr(const bstring* s1, int pos, const bstring* s2)`
  + Search for the bstring `b2` in `b1` starting from position `pos`, and searching backward.  If it is found then return with the first position where it is found, otherwise return `BSTR_ERR`.  Note that this is just a brute force string searcher that does not attempt clever things like the Boyer-Moore search algorithm.  Because of this there are many degenerate cases where this can take much longer than it needs to.
+ `int binstrcaseless(const bstring* s1, int pos, const bstring* s2)`
  + Search for the bstring `b2` in `b1` starting from position `pos`, and searching forward but without regard to case.  If it is found then return with the first position where it is found, otherwise return `BSTR_ERR`.  Note that this is just a brute force string searcher that does not attempt clever things like the Boyer-Moore search algorithm.  Because of this there are many degenerate cases where this can take much longer than it needs to.
+ `int binstrrcaseless(const bstring* s1, int pos, const bstring* s2)`
  + Search for the bstring `b2` in `b1` starting from position `pos`, and searching backward but without regard to case.  If it is found then return with the first position where it is found, otherwise return `BSTR_ERR`.  Note that this is just a brute force string searcher that does not attempt clever things like the Boyer-Moore search algorithm.  Because of this there are many degenerate cases where this can take much longer than it needs to.

### Character search
Functions below can't be used to search for `\0` that is outside of strings size. For example if you have string `"foo"` with capacity 30 it won't check the 4th character. This was done to make bstrings properly work with non-null terminated strings

+ `int bstrchr(const bstring* b, int c)`
  + Search for the character `c` in the bstring `b` forwards from the start of
    the bstring.  Returns the position of the found character or `BSTR_ERR` if
    it is not found
+ `int bstrchrp(const bstring* b, int c, int pos)`
  + Search for the character `c` in `b` forwards from the position `pos` (inclusive)
+ `int bstrrchr(const bstring* b, int c)`
  + Search for the character `c` in the bstring `b` backwards from the end of the bstring.  Returns the position of the found character or `BSTR_ERR` if it is not found
+ `int bstrrchrp(const bstring* b, int c, int pos)`
  + Search for the character `c` in `b` backwards from the position `pos` in string (inclusive)
+ `int binchr(const bstring* b0, int pos, const bstring* b1)`
  + Search for the first position in `b0` greater or equal than `pos`. In which one of the characters from `b1` is found and return it.  If such a position does not exist in `b0`, then `BSTR_ERR` is returned
+ `int binchrr(const bstring* b0, int pos, const bstring* b1)`
  + Search for the last position in `b0` less or equal than `pos`. In which one of the characters from `b1` is found and return it.  If such a position does not exist in `b0`, then `BSTR_ERR` is returned.
  + `pos` is clamped to `b0->slen - 1`
+ `int bninchr(const bstring* b0, int pos, const bstring* b1)`
  + Search for the first position in `b0` greater or equal than `pos`, in which none of the characters from `b1` are found and return it.  If such a position does not exist in `b0`, then `BSTR_ERR` is returned.
+ `int bninchrr(const bstring* b0, int pos, const bstring* b1)`
  + Search for the last position in `b0` less or equal than `pos`, in which none of the characters in `b1` is found and return it.  If such a position does not exist in `b0`, then `BSTR_ERR` is returned.

## Format functions (similar to sprintf)

**Note:** functions below do not support aliasing pointers. I.e. you can't do the following:
```c
bstring* str = ...;
bformat(str, "%s", (const char*)str->data)
```

+ `int bformat(bstring* b, const char* fmt, ...)`
  + Acts same way as `sprintf` but will reallocate underlying buffer if needed
+ `int bformata(bstring* b, const char* fmt, ...)`
  + Appends formatted string to b, instead of overwriting existing data
+ `int bvformat(bstring* b, const char* fmt, va_list args)`
  + Same as `bformat` but uses `va_list`
+ `int bvformata(bstring* b, const char* fmt, va_list args)`
  + Same as `bformata` but uses `va_list`