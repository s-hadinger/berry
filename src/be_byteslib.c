/********************************************************************
** Copyright (c) 2018-2020 Guan Wenliang - Stephan Hadinger
** This file is part of the Berry default interpreter.
** skiars@qq.com, https://github.com/Skiars/berry
** See Copyright Notice in the LICENSE file or at
** https://github.com/Skiars/berry/blob/master/LICENSE
********************************************************************/
#include "be_object.h"
#include "be_string.h"
#include "be_strlib.h"
#include "be_list.h"
#include "be_func.h"
#include "be_exec.h"
#include "be_vm.h"
#include "be_mem.h"
#include <string.h>
#include <ctype.h>

#define BYTES_DEFAULT_SIZE          28              // default pre-reserved size for buffer (keep 4 bytes for len/size)
#define BYTES_MAX_SIZE              (32*1024)       // max 32Kb
#define BYTES_OVERHEAD              4               // bytes overhead to be added when allocating (used to store len and size)
#define BYTES_HEADROOM              8               // keep a natural headroom of 8 bytes when resizing

typedef struct buf_impl {
  uint16_t size;                // size in bytes of the buffer
  uint16_t len;                 // current size of the data in buffer. Invariant: len <= size
  uint8_t buf[];                // the actual data
} buf_impl;

/********************************************************************
** Buffer low-level implementation
**
** Extracted from Tasmota SBuffer lib
********************************************************************/
static inline uint8_t* buf_get_buf(buf_impl* buf) {
    return &buf->buf[0];
}

// shrink or increase. If increase, fill with zeores. Cannot go beyond `size`
static void buf_set_len(buf_impl* buf, const size_t len) {
    uint16_t old_len = buf->len;
    buf->len = (len <= buf->size) ? len : buf->size;
    if (old_len < buf->len) {
        memset((void*) &buf->buf[old_len], 0, buf->len - old_len);
    }
}


static size_t buf_add1(buf_impl* buf, const uint8_t data) {           // append 8 bits value
    if (buf->len < buf->size) {       // do we have room for 1 byte
        buf->buf[buf->len++] = data;
    }
    return buf->len;
}
static size_t buf_add2_le(buf_impl* buf, const uint16_t data) {           // append 16 bits value
    if (buf->len < buf->size - 1) {    // do we have room for 2 bytes
        buf->buf[buf->len++] = data;
        buf->buf[buf->len++] = data >> 8;
    }
    return buf->len;
}
static size_t buf_add2_be(buf_impl* buf, const uint16_t data) {           // append 16 bits value
    if (buf->len < buf->size - 1) {    // do we have room for 2 bytes
        buf->buf[buf->len++] = data >> 8;
        buf->buf[buf->len++] = data;
    }
    return buf->len;
}
static size_t buf_add4_le(buf_impl* buf, const uint32_t data) {           // append 32 bits value
    if (buf->len < buf->size - 3) {     // do we have room for 4 bytes
        buf->buf[buf->len++] = data;
        buf->buf[buf->len++] = data >> 8;
        buf->buf[buf->len++] = data >> 16;
        buf->buf[buf->len++] = data >> 24;
    }
    return buf->len;
}
size_t buf_add4_be(buf_impl* buf, const uint32_t data) {           // append 32 bits value
    if (buf->len < buf->size - 3) {     // do we have room for 4 bytes
        buf->buf[buf->len++] = data >> 24;
        buf->buf[buf->len++] = data >> 16;
        buf->buf[buf->len++] = data >> 8;
        buf->buf[buf->len++] = data;
    }
    return buf->len;
}

static size_t buf_add_buf(buf_impl* buf, buf_impl* buf2) {
    if (buf->len + buf2->len <= buf->size) {
        for (uint32_t i = 0; i < buf2->len; i++) {
            buf->buf[buf->len++] = buf2->buf[i];
        }
    }
    return buf->len;
}

static uint8_t buf_get1(buf_impl* buf, int offset) {
    if ((offset >= 0) && (offset < buf->len)) {
        return buf->buf[offset];
    }
    return 0;
}
static uint16_t buf_get2_le(buf_impl* buf, int offset) {
    if ((offset >= 0) && (offset < buf->len - 1)) {
        return buf->buf[offset] | (buf->buf[offset+1] << 8);
    }
    return 0;
}
static uint16_t buf_get2_be(buf_impl* buf, int offset) {
    if (offset < buf->len - 1) {
        return buf->buf[offset+1] | (buf->buf[offset] << 8);
    }
    return 0;
}
static uint32_t buf_get4_le(buf_impl* buf, int offset) {
    if ((offset >= 0) && (offset < buf->len - 3)) {
        return buf->buf[offset] | (buf->buf[offset+1] << 8) |
            (buf->buf[offset+2] << 16) | (buf->buf[offset+3] << 24);
    }
    return 0;
}
static uint32_t buf_get4_be(buf_impl* buf, int offset) {
    if (offset < buf->len - 3) {
        return buf->buf[offset+3] | (buf->buf[offset+2] << 8) |
            (buf->buf[offset+1] << 16) | (buf->buf[offset] << 24);
    }
    return 0;
}

// nullptr accepted
static bbool buf_equals(buf_impl* buf1, buf_impl* buf2) {
    if (buf1 == buf2) { return btrue; }
    if (!buf1 || !buf2) { return bfalse; }   // at least one buf is not empty
    // we know that both buf1 and buf2 are non-null
    if (buf1->len != buf2->len) { return bfalse; }
    size_t len = buf1->len;
    for (uint32_t i=0; i<len; i++) {
        if (buf_get1(buf1, i) != buf_get1(buf2, i)) { return bfalse; }
    }
    return btrue;
}

static uint8_t asc2byte(char chr) {
    uint8_t rVal = 0;
    if (isdigit(chr)) { rVal = chr - '0'; }
    else if (chr >= 'A' && chr <= 'F') { rVal = chr + 10 - 'A'; }
    else if (chr >= 'a' && chr <= 'f') { rVal = chr + 10 - 'a'; }
    return rVal;
}
// does not check if there is enough room before hand, truncated if buffer too small
static void buf_add_hex(buf_impl* buf, const char *hex, size_t len) {
    uint8_t val;
    for (; len > 1; len -= 2) {
        val = asc2byte(*hex++) << 4;
        val |= asc2byte(*hex++);
        buf_add1(buf, val);
    }
}

/********************************************************************
** Wrapping into lib
********************************************************************/
// typedef int (*bntvfunc)(bvm*); /* native function pointer */
int free_bytes_buf(bvm* vm) {
    int argc = be_top(vm);
    if (argc > 0) {
        buf_impl * buf = (buf_impl*) be_tocomptr(vm, 1);
        if (buf != NULL) {
            be_os_free(buf);
        }
    }
    be_return_nil(vm);
}

buf_impl * bytes_alloc(int32_t size) {
    if (size < 4) { size = 4; }
    if (size > BYTES_MAX_SIZE) { size = BYTES_MAX_SIZE; }
    buf_impl * next = (buf_impl*) be_os_malloc(size + BYTES_OVERHEAD);
    next->size = size;
    next->len = 0;
    return next;
}

static int m_init(bvm *vm)
{
    int argc = be_top(vm);
    int size = BYTES_DEFAULT_SIZE;
    const char * hex_in = NULL;
    if (argc > 1 && be_isstring(vm, 2)) {
        hex_in = be_tostring(vm, 2);
        if (hex_in) {
            size = strlen(hex_in) / 2 + BYTES_HEADROOM;        // allocate headroom
        }
    }
    buf_impl * buf = bytes_alloc(size);
    if (!buf) {
        be_throw(vm, BE_MALLOC_FAIL);
    }
    
    if (hex_in) {
        buf_add_hex(buf, hex_in, strlen(hex_in));
    } else if (argc > 1 && be_isint(vm, 2)) {
        buf_add1(buf, be_toint(vm, 2));
    }
    be_newcomobj(vm, buf, &free_bytes_buf);
    be_setmember(vm, 1, ".p");
    be_return_nil(vm);
}

/* grow or shrink to the exact value */
/* stack item 1 must contain the instance */
static buf_impl * _bytes_resize(bvm *vm, buf_impl * buf, size_t new_size) {
    buf_impl * new_buf = bytes_alloc(new_size);
    if (!new_buf) {
        be_throw(vm, BE_MALLOC_FAIL);
    }
    memmove(buf_get_buf(new_buf), buf_get_buf(buf), buf->len);
    new_buf->len = buf->len;
    /* replace the .p attribute */
    be_newcomobj(vm, new_buf, &free_bytes_buf);
    be_setmember(vm, 1, ".p");
    be_pop(vm, 1); /* remove comobj from stack */
    /* the old buffer will be garbage collected later */
    return new_buf;
}

/* grow if needed but don't shrink */
/* if grow, then add some headroom */
/* stack item 1 must contain the instance */
static buf_impl * bytes_resize(bvm *vm, buf_impl * buf, size_t new_size) {
    if (buf->size >= new_size) { return buf; }  /* no resize needed */
    return _bytes_resize(vm, buf, new_size + BYTES_HEADROOM);
}

static buf_impl * bytes_check_data(bvm *vm, size_t add_size) {
    be_getmember(vm, 1, ".p");
    buf_impl * buf = be_tocomptr(vm, -1);
    be_pop(vm, 1); /* remove member from stack */
    /* check if the `size` is big enough */
    if (buf->len + add_size > buf->size) {
        /* it does not fit so we need to realocate the buffer */
        buf = bytes_resize(vm, buf, buf->len + add_size);
    }
    return buf;
}

static size_t tohex(char * out, size_t outsz, const uint8_t * in, size_t insz) {
  static const char * hex = "0123456789ABCDEF";
  const uint8_t * pin = in;
  char * pout = out;
  for (; pin < in + insz; pout += 2, pin++) {
    pout[0] = hex[((*pin)>>4) & 0xF];
    pout[1] = hex[ (*pin)     & 0xF];
    if (pout + 3 > out + outsz) { break; } /* check overflow */
  }
  pout[0] = 0; /* terminating Nul char */
  return pout - out;
}
/*
 * Copy the buffer into a string without any changes
 */
static int m_tostring(bvm *vm)
{
    buf_impl * buf = bytes_check_data(vm, 0);
    size_t len = buf->len;
    size_t hex_len = len * 2 + 5 + 2 + 2 + 1;  /* reserve size for `bytes("")\0` - 9 chars */

    char * hex_out = be_pushbuffer(vm, hex_len);
    size_t l = strlcpy(hex_out, "bytes(\"", hex_len);
    l += tohex(&hex_out[l], hex_len - l, buf_get_buf(buf), buf->len);
    l += strlcpy(&hex_out[l], "\")", hex_len - l);

    be_pushnstring(vm, hex_out, l); /* make escape string from buffer */
    be_remove(vm, -2); /* remove buffer */
    be_return(vm);
}

/*
 * Add an int made of 1, 2 or 4 bytes, in little or big endian
 * `add(value:int[, size:int = 1]) -> instance`
 * 
 * size: may be 1, 2, 4 (little endian), or -1, -2, -4 (big endian)
 *       obvisouly -1 is idntical to 1
 *       size==0 does nothing
 */
static int m_add(bvm *vm)
{
    int argc = be_top(vm);
    buf_impl * buf = bytes_check_data(vm, 4); /* we reserve 4 bytes anyways */
    if (argc >=2 && be_isint(vm, 2)) {
        int32_t v = be_toint(vm, 2);
        int vsize = 1;
        if (argc >= 3 && be_isint(vm, 3)) {
            vsize = be_toint(vm, 3);
        }
        switch (vsize) {
            case 0:                             break;
            case -1:    /* fallback below */
            case 1:     buf_add1(buf, v);       break;
            case 2:     buf_add2_le(buf, v);    break;
            case 4:     buf_add4_le(buf, v);    break;
            case -2:    buf_add2_be(buf, v);    break;
            case -4:    buf_add4_be(buf, v);    break;
            default:    be_raise(vm, "type_error", "size must be -4, -2, -1, 0, 1, 2 or 4.");
        }
        be_pop(vm, argc - 1);
        be_return(vm);
    }
    be_return_nil(vm);
}

/*
 * Get an int made of 1, 2 or 4 bytes, in little or big endian
 * `get(index:int[, size:int = 1]) -> instance`
 * 
 * size: may be 1, 2, 4 (little endian), or -1, -2, -4 (big endian)
 *       obvisouly -1 is idntical to 1
 *       0 returns nil
 */
static int m_get(bvm *vm)
{
    int argc = be_top(vm);
    buf_impl * buf = bytes_check_data(vm, 0); /* we reserve 4 bytes anyways */
    if (argc >=2 && be_isint(vm, 2)) {
        int32_t idx = be_toint(vm, 2);
        int vsize = 1;
        if (argc >= 3 && be_isint(vm, 3)) {
            vsize = be_toint(vm, 3);
        }
        int ret = 0;
        switch (vsize) {
            case 0:                                     break;
            case -1:    /* fallback below */
            case 1:     ret = buf_get1(buf, idx);       break;
            case 2:     ret = buf_get2_le(buf, idx);    break;
            case 4:     ret = buf_get4_le(buf, idx);    break;
            case -2:    ret = buf_get2_be(buf, idx);    break;
            case -4:    ret = buf_get4_be(buf, idx);    break;
            default:    be_raise(vm, "type_error", "size must be -4, -2, -1, 0, 1, 2 or 4.");
        }
        be_pop(vm, argc - 1);
        if (vsize != 0) {
            be_pushint(vm, ret);
        } else {
            be_pushnil(vm);
        }
        be_return(vm);
    }
    be_return_nil(vm);
}

static int m_size(bvm *vm)
{
    buf_impl * buf = bytes_check_data(vm, 0);
    be_pushint(vm, buf->len);
    be_return(vm);
}

static int m_resize(bvm *vm)
{
    int argc = be_top(vm);
    be_getmember(vm, 1, ".p");
    buf_impl * buf = be_tocomptr(vm, -1);
    be_pop(vm, 1);

    if (argc <= 1 || !be_isint(vm, 2)) {
        be_raise(vm, "type_error", "size must be of type 'int'");
    }
    int new_len = be_toint(vm, 2);
    if (new_len < 0) {
        new_len = 0;
    }

    buf = bytes_resize(vm, buf, new_len);
    buf_set_len(buf, new_len);
    be_pop(vm, 1);
    be_return(vm);
}

static int m_merge(bvm *vm)
{
    int argc = be_top(vm);
    buf_impl * buf1 = bytes_check_data(vm, 0); /* no resize yet */
    if (argc >= 2 && be_isinstance(vm, 2)) {
        be_getglobal(vm, "bytes"); /* get the bytes class */ /* TODO eventually replace with be_getbuiltin */
        if (be_isderived(vm, 2)) {
            be_pop(vm, 1);  /* remove class */
            be_getmember(vm, 2, ".p");
            buf_impl * buf2 = be_tocomptr(vm, -1);
            be_pop(vm, 3); /* remove member, and 2 operands */

            /* allocate new object */
            be_pushint(vm, buf1->len + buf2->len);
            be_newobject(vm, "bytes");
            /* .p is on top of stack, then instance */
            buf_impl * buf3 = be_tocomptr(vm, -1);
            be_pop(vm, 1);
            buf_add_buf(buf3, buf1);
            buf_add_buf(buf3, buf2);

            be_return(vm); /* return self */
        }
    }
    be_raise(vm, "type_error", "operand must be bytes");
    be_return_nil(vm); /* return self */
}

/* accept bytes or int as operand */
static int m_connect(bvm *vm)
{
    int argc = be_top(vm);
    buf_impl * buf1 = bytes_check_data(vm, 0); /* don't resize yet */
    if (argc >= 2 && (be_isinstance(vm, 2) || be_isint(vm, 2))) {
        if (be_isint(vm, 2)) {
            buf1 = bytes_resize(vm, buf1, buf1->len + 1); /* resize */
            buf_add1(buf1, be_toint(vm, 2));
            be_pop(vm, 1);  /* remove operand */
            be_return(vm); /* return self */
        } else {
            be_getglobal(vm, "bytes"); /* get the bytes class */ /* TODO eventually replace with be_getbuiltin */
            if (be_isderived(vm, 2)) {
                be_getmember(vm, 2, ".p");
                buf_impl * buf2 = be_tocomptr(vm, -1);
                buf1 = bytes_resize(vm, buf1, buf1->len + buf2->len); /* resize buf1 for total size */
                buf_add_buf(buf1, buf2);
                be_pop(vm, 3); /* remove class, member, and last operand */
                be_return(vm); /* return self */
            }
        }
    }
    be_raise(vm, "type_error", "operand must be bytes or int");
    be_return_nil(vm); /* return self */
}

static int bytes_equal(bvm *vm, bbool iseq)
{
    be_getmember(vm, 1, ".p");
    buf_impl * buf1 = be_tocomptr(vm, -1);
    be_pop(vm, 1);

    be_getmember(vm, 2, ".p");
    buf_impl * buf2 = be_tocomptr(vm, -1);
    be_pop(vm, 1);

    bbool ret;
    if (buf_equals(buf1, buf2)) {
        ret = iseq;
    } else {
        ret = !iseq;
    }
    be_pushbool(vm, ret);
    be_return(vm);
}

static int m_equal(bvm *vm)
{
    return bytes_equal(vm, btrue);
}

static int m_nequal(bvm *vm)
{
    return bytes_equal(vm, bfalse);
}

#if !BE_USE_PRECOMPILED_OBJECT
void be_load_byteslib(bvm *vm)
{
    static const bnfuncinfo members[] = {
        { ".p", NULL },
        { "init", m_init },
        { "tostring", m_tostring },
        { "add", m_add },
        { "get", m_get },
        { "size", m_size },
        { "resize", m_resize },
        { "+", m_merge },
        { "..", m_connect },
        { "==", m_equal },
        { "!=", m_nequal },
        { NULL, NULL }
    };
    be_regclass(vm, "bytes", members);
}
#else
/* @const_object_info_begin
class be_class_bytes (scope: global, name: bytes) {
    .p, var
    init, func(m_init)
    tostring, func(m_tostring)
    add, func(m_add)
    get, func(m_get)
    size, func(m_size)
    resize, func(m_resize)
    +, func(m_merge)
    .., func(m_connect)
    ==, func(m_equal)
    !=, func(m_nequal)
}
@const_object_info_end */
#include "../generate/be_fixed_be_class_bytes.h"
#endif