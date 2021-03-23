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

be_define_local_const_str(f_str_name, "f", 0, 0, 1, 0);
be_define_local_const_str(f_str_source, "stdin", 0, 0, 5, 0);
be_define_local_const_str(f_str_3, "Hello", 0, 0, 5, 0);

static const bvalue f_ktab[4] = {
  { { .i=-1234567890123456 }, BE_INT},
  { { .p=(void*)0x4000000000000000 }, BE_REAL},
  { { .p=(void*)0x3FA111111111110E }, BE_REAL},
  { { .s=be_local_const_str(f_str_3) }, BE_STRING},
};

static const uint32_t f_code[7] = {
  0x6008000F,  //  0000  GETGBL	R2	G15
  0x580C0000,  //  0001  LDCONST	R3	K0
  0x7C080200,  //  0002  CALL	R2	1
  0x6008000F,  //  0003  GETGBL	R2	G15
  0xC0E0302,  //  0004  DIV	R3	R257	R258
  0x7C080200,  //  0005  CALL	R2	1
  0x80060600,  //  0006  RET	1	R259
};

static const bproto f_proto = {
  NULL,     // bgcobject *next
  8,       // type
  GC_CONST,        // marked
  4,       // nstack
  0,       // nupvals
  2,       // argc
  0,       // varg
  NULL,     // bgcobject *gray
  NULL,     // bupvaldesc *upvals
  (bvalue*) &f_ktab,     // ktab
  NULL,     // bproto **ptab
  (binstruction*) &f_code,     // code
  be_local_const_str(f_str_name),       // name
  7,       // codesize
  4,       // nconst
  0,       // nproto
  be_local_const_str(f_str_source),     // source
#if BE_DEBUG_RUNTIME_INFO /* debug information */
  NULL,     // lineinfo
  0,        // nlineinfo
#endif
#if BE_DEBUG_VAR_INFO
  NULL,     // varinfo
  0,        // nvarinfo
#endif
};

const bclosure f_closure = {
  NULL,     // bgcobject *next
  36,       // type
  GC_CONST,        // marked
  0,       // nupvals
  NULL,     // bgcobject *gray
  (bproto*) &f_proto,     // proto
  { NULL }     // upvals
};

/*******************************************************************/




/********************************************************************
** Solidified function: event
********************************************************************/

be_define_local_const_str(event_str_name, "event", 0, 0, 5, 0);
be_define_local_const_str(event_str_source, "stdin", 0, 0, 5, 0);
be_define_local_const_str(event_str_0, "cmd", 0, 0, 3, 0);
be_define_local_const_str(event_str_1, "exec_cmd", 0, 0, 8, 0);
be_define_local_const_str(event_str_2, "rule", 0, 0, 4, 0);
be_define_local_const_str(event_str_3, "exec_rules", 0, 0, 10, 0);
be_define_local_const_str(event_str_4, "mqtt_data", 0, 0, 9, 0);
be_define_local_const_str(event_str_5, "gc", 0, 0, 2, 0);
be_define_local_const_str(event_str_6, "every_50ms", 0, 0, 10, 0);
be_define_local_const_str(event_str_7, "run_deferred", 0, 0, 12, 0);
be_define_local_const_str(event_str_8, "_drivers", 0, 0, 8, 0);
be_define_local_const_str(event_str_9, "dispatch", 0, 0, 8, 0);
be_define_local_const_str(event_str_10, "every_second", 0, 0, 12, 0);
be_define_local_const_str(event_str_11, "every_100ms", 0, 0, 11, 0);
be_define_local_const_str(event_str_12, "string", 0, 0, 6, 0);
be_define_local_const_str(event_str_13, "log", 0, 0, 3, 0);
be_define_local_const_str(event_str_14, "format", 0, 0, 6, 0);
be_define_local_const_str(event_str_15, "BRY: exception %s - %m", 0, 0, 22, 0);
be_define_local_const_str(event_str_17, "stop_iteration", 0, 0, 14, 0);

static const bvalue event_ktab[18] = {
  { { .s=be_local_const_str(event_str_0) }, BE_STRING},
  { { .s=be_local_const_str(event_str_1) }, BE_STRING},
  { { .s=be_local_const_str(event_str_2) }, BE_STRING},
  { { .s=be_local_const_str(event_str_3) }, BE_STRING},
  { { .s=be_local_const_str(event_str_4) }, BE_STRING},
  { { .s=be_local_const_str(event_str_5) }, BE_STRING},
  { { .s=be_local_const_str(event_str_6) }, BE_STRING},
  { { .s=be_local_const_str(event_str_7) }, BE_STRING},
  { { .s=be_local_const_str(event_str_8) }, BE_STRING},
  { { .s=be_local_const_str(event_str_9) }, BE_STRING},
  { { .s=be_local_const_str(event_str_10) }, BE_STRING},
  { { .s=be_local_const_str(event_str_11) }, BE_STRING},
  { { .s=be_local_const_str(event_str_12) }, BE_STRING},
  { { .s=be_local_const_str(event_str_13) }, BE_STRING},
  { { .s=be_local_const_str(event_str_14) }, BE_STRING},
  { { .s=be_local_const_str(event_str_15) }, BE_STRING},
  { { .i=3 }, BE_INT},
  { { .s=be_local_const_str(event_str_17) }, BE_STRING},
};

static const uint32_t event_code[87] = {
  0x1C140300,  //  0000  EQ	R5	R1	R256
  0x78160006,  //  0001  JMPF	R5	#0009
  0x8C140101,  //  0002  GETMET	R5	R0	R257
  0x5C1C0400,  //  0003  MOVE	R7	R2
  0x5C200600,  //  0004  MOVE	R8	R3
  0x5C240800,  //  0005  MOVE	R9	R4
  0x7C140800,  //  0006  CALL	R5	4
  0x80040A00,  //  0007  RET	1	R5
  0x7002004C,  //  0008  JMP		#0056
  0x1C140302,  //  0009  EQ	R5	R1	R258
  0x78160004,  //  000A  JMPF	R5	#0010
  0x8C140103,  //  000B  GETMET	R5	R0	R259
  0x5C1C0800,  //  000C  MOVE	R7	R4
  0x7C140400,  //  000D  CALL	R5	2
  0x80040A00,  //  000E  RET	1	R5
  0x70020045,  //  000F  JMP		#0056
  0x1C140304,  //  0010  EQ	R5	R1	R260
  0x78160002,  //  0011  JMPF	R5	#0015
  0x4C140000,  //  0012  LDNIL	5
  0x80040A00,  //  0013  RET	1	R5
  0x70020040,  //  0014  JMP		#0056
  0x1C140305,  //  0015  EQ	R5	R1	R261
  0x78160003,  //  0016  JMPF	R5	#001B
  0x8C140105,  //  0017  GETMET	R5	R0	R261
  0x7C140200,  //  0018  CALL	R5	1
  0x80040A00,  //  0019  RET	1	R5
  0x7002003A,  //  001A  JMP		#0056
  0x1C140306,  //  001B  EQ	R5	R1	R262
  0x78160003,  //  001C  JMPF	R5	#0021
  0x8C140107,  //  001D  GETMET	R5	R0	R263
  0x7C140200,  //  001E  CALL	R5	1
  0x80040A00,  //  001F  RET	1	R5
  0x70020034,  //  0020  JMP		#0056
  0x88140108,  //  0021  GETMBR	R5	R0	R264
  0x78160032,  //  0022  JMPF	R5	#0056
  0x60140010,  //  0023  GETGBL	R5	G16
  0x88180108,  //  0024  GETMBR	R6	R0	R264
  0x7C140200,  //  0025  CALL	R5	1
  0xA802002B,  //  0026  EXBLK	0	#0053
  0x5C180A00,  //  0027  MOVE	R6	R5
  0x7C180000,  //  0028  CALL	R6	0
  0xA802001C,  //  0029  EXBLK	0	#0047
  0x881C0D09,  //  002A  GETMBR	R7	R6	R265
  0x781E0007,  //  002B  JMPF	R7	#0034
  0x8C1C0D09,  //  002C  GETMET	R7	R6	R265
  0x5C240200,  //  002D  MOVE	R9	R1
  0x5C280400,  //  002E  MOVE	R10	R2
  0x5C2C0600,  //  002F  MOVE	R11	R3
  0x5C300800,  //  0030  MOVE	R12	R4
  0x7C1C0A00,  //  0031  CALL	R7	5
  0x781E0000,  //  0032  JMPF	R7	#0034
  0x70020010,  //  0033  JMP		#0045
  0x1C1C030A,  //  0034  EQ	R7	R1	R266
  0x781E0006,  //  0035  JMPF	R7	#003D
  0x881C0D0A,  //  0036  GETMBR	R7	R6	R266
  0x781E0004,  //  0037  JMPF	R7	#003D
  0x8C1C0D0A,  //  0038  GETMET	R7	R6	R266
  0x7C1C0200,  //  0039  CALL	R7	1
  0xA8040002,  //  003A  EXBLK	1	2
  0x80040E00,  //  003B  RET	1	R7
  0x70020007,  //  003C  JMP		#0045
  0x1C1C030B,  //  003D  EQ	R7	R1	R267
  0x781E0005,  //  003E  JMPF	R7	#0045
  0x881C0D0B,  //  003F  GETMBR	R7	R6	R267
  0x781E0003,  //  0040  JMPF	R7	#0045
  0x8C1C0D0B,  //  0041  GETMET	R7	R6	R267
  0x7C1C0200,  //  0042  CALL	R7	1
  0xA8040002,  //  0043  EXBLK	1	2
  0x80040E00,  //  0044  RET	1	R7
  0xA8040001,  //  0045  EXBLK	1	1
  0x7002000A,  //  0046  JMP		#0052
  0xAC1C0002,  //  0047  CATCH	R7	0	2
  0x70020007,  //  0048  JMP		#0051
  0xA4261800,  //  0049  IMPORT	R9	R268
  0x8C28010D,  //  004A  GETMET	R10	R0	R269
  0x8C30130E,  //  004B  GETMET	R12	R9	R270
  0x5838000F,  //  004C  LDCONST	R14	K15
  0x583C0010,  //  004D  LDCONST	R15	K16
  0x7C300600,  //  004E  CALL	R12	3
  0x7C280400,  //  004F  CALL	R10	2
  0x70020000,  //  0050  JMP		#0052
  0xB0080000,  //  0051  RAISE	2	R0	R0
  0x7001FFD3,  //  0052  JMP		#0027
  0x58140011,  //  0053  LDCONST	R5	K17
  0xAC140200,  //  0054  CATCH	R5	1	0
  0xB0080000,  //  0055  RAISE	2	R0	R0
  0x80000000,  //  0056  RET	0	R0
};

static const bproto event_proto = {
  NULL,     // bgcobject *next
  8,       // type
  GC_CONST,        // marked
  16,       // nstack
  0,       // nupvals
  5,       // argc
  0,       // varg
  NULL,     // bgcobject *gray
  NULL,     // bupvaldesc *upvals
  (bvalue*) &event_ktab,     // ktab
  NULL,     // bproto **ptab
  (binstruction*) &event_code,     // code
  be_local_const_str(event_str_name),       // name
  87,       // codesize
  18,       // nconst
  0,       // nproto
  be_local_const_str(event_str_source),     // source
#if BE_DEBUG_RUNTIME_INFO /* debug information */
  NULL,     // lineinfo
  0,        // nlineinfo
#endif
#if BE_DEBUG_VAR_INFO
  NULL,     // varinfo
  0,        // nvarinfo
#endif
};

const bclosure event_closure = {
  NULL,     // bgcobject *next
  36,       // type
  GC_CONST,        // marked
  0,       // nupvals
  NULL,     // bgcobject *gray
  (bproto*) &event_proto,     // proto
  { NULL }     // upvals
};

/*******************************************************************/






// #if !BE_USE_PRECOMPILED_OBJECT
#if 1           // TODO we will do pre-compiled later
void be_load_testlib(bvm *vm)
{
    static const bnfuncinfo members[] = {
        { "dummy", NULL },               // bus number

        { NULL, (bntvfunc) BE_CLOSURE },
        { "f", (bntvfunc) &f_closure },
        { "event", (bntvfunc) &event_closure },
        
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
