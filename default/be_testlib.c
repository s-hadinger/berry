/********************************************************************
 * Test lib
 * 
 *******************************************************************/
#include "be_object.h"
#include "be_string.h"
#include "be_gc.h"
#include "../generate/be_const_strtab.h"


/********************************************************************
** Solidified function: f
********************************************************************/

/********** Solidified proto: f_0 */
static const bupvaldesc f_0_upvals[1] = {
  be_local_const_upval(1, 1),
};

be_define_local_const_str(f_0_str_name, "f_0", 607256038, 0, 8, 0);
be_define_local_const_str(f_0_str_source, "stdin", -1529146723, 0, 5, 0);

static const uint32_t f_0_code[4] = {
  0x6000000F,  //  0000  GETGBL R0 G15
  0x68040000,  //  0001  GETUPV R1 U0
  0x7C000200,  //  0002  CALL R0 1
  0x80040000,  //  0003  RET 1 R0
};

be_define_local_proto(f_0, 2, 0, 0, 0, 1);

/********** Solidified proto: f */
static const bproto *f_subproto[1] = {
  &f_0_proto,
};

be_define_local_const_str(f_str_name, "f", -485742695, 0, 1, 0);
be_define_local_const_str(f_str_source, "stdin", -1529146723, 0, 5, 0);

static const uint32_t f_code[3] = {
  0x84080000,  //  0000  CLOSURE R2 P0
  0xA0000000,  //  0001  CLOSE 0
  0x80040400,  //  0002  RET 1 R2
};

be_define_local_proto(f, 3, 2, 0, 1, 0);
be_define_local_closure(f);

/*******************************************************************/


// #if !BE_USE_PRECOMPILED_OBJECT
#if 1           // TODO we will do pre-compiled later
void be_load_testlib(bvm *vm)
{
    static const bnfuncinfo members[] = {
        { "dummy", NULL },               // bus number

        { NULL, (bntvfunc) BE_CLOSURE },
        { "f", (bntvfunc) &f_closure },
        
        { NULL, NULL }
    };
    be_regclass(vm, "test", members);
}
#else
/* @const_object_info_begin
module tasmota (scope: global, depend: 1) {
    dummy, var
    f, closure(f)
    event, closure(event)
}
@const_object_info_end */
#include "../generate/be_fixed_test.h"
#endif
