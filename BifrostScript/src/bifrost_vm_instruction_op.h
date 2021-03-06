/******************************************************************************/
/*!
 * @file   bifrost_vm_instruction_op.h
 * @author Shareef Abdoul-Raheem (http://blufedora.github.io/)
 * @par
 *    Bifrost Scripting Language
 *
 * @brief
 *    The list of op codes the virtual machine handles along with
 *    the spec on interpretting each code.
 *
 * @version 0.0.1-beta
 * @date    2019-07-01
 *
 * @copyright Copyright (c) 2019 Shareef Raheem
 */
/******************************************************************************/
#ifndef BIFROST_VM_INSTRUCTION_OP_H
#define BIFROST_VM_INSTRUCTION_OP_H

#include <stdint.h> /* uint32_t */

#if __cplusplus
extern "C" {
#endif

#define BIFROST_VM_OP_LOAD_BASIC_TRUE 0
#define BIFROST_VM_OP_LOAD_BASIC_FALSE 1
#define BIFROST_VM_OP_LOAD_BASIC_NULL 2
#define BIFROST_VM_OP_LOAD_BASIC_CURRENT_MODULE 3
#define BIFROST_VM_OP_LOAD_BASIC_CONSTANT 4

// Total of 25 / 31 possible ops.
typedef enum bfInstructionOp_t
{
  /* Load OPs */
  BIFROST_VM_OP_LOAD_SYMBOL,  // rA = rB.SYMBOLS[rC]
  BIFROST_VM_OP_LOAD_BASIC,   // rA = (rBx == 0 : VAL_TRUE) || (rBx == 1 : VAL_FALSE) || (rBx == 2 : VAL_NULL) || (rBx == 3 : <current-module>) || (rBx > 3 : K[rBx - 4])

  /* Store OPs */
  BIFROST_VM_OP_STORE_MOVE,    // rA              = rBx
  BIFROST_VM_OP_STORE_SYMBOL,  // rA.SYMBOLS[rB]  = rC

  /* Memory OPs */
  BIFROST_VM_OP_NEW_CLZ,  // rA = new local[rBx];

  // Math OPs
  BIFROST_VM_OP_MATH_ADD,  // rA = rB + rC
  BIFROST_VM_OP_MATH_SUB,  // rA = rB - rC
  BIFROST_VM_OP_MATH_MUL,  // rA = rB * rC
  BIFROST_VM_OP_MATH_DIV,  // rA = rB / rC
  BIFROST_VM_OP_MATH_MOD,  // rA = rB % rC
  BIFROST_VM_OP_MATH_POW,  // rA = rB ^ rC
  BIFROST_VM_OP_MATH_INV,  // rA = -rB

  // Additional Logical Ops
  // BIFROST_VM_OP_LOGIC_OR,   // rA = rB | rC
  // BIFROST_VM_OP_LOGIC_AND,  // rA = rB & rC
  // BIFROST_VM_OP_LOGIC_XOR,  // rA = rB ^ rC
  // BIFROST_VM_OP_LOGIC_NOT,  // rA = ~rB
  // BIFROST_VM_OP_LOGIC_LS,   // rA = (rB << rC)
  // BIFROST_VM_OP_LOGIC_RS,   // rA = (rB >> rC)

  // Comparisons
  BIFROST_VM_OP_CMP_EE,  /* rA = rB == rC */
  BIFROST_VM_OP_CMP_NE,  /* rA = rB != rC */
  BIFROST_VM_OP_CMP_LT,  /* rA = rB <  rC */
  BIFROST_VM_OP_CMP_LE,  /* rA = rB <= rC */
  BIFROST_VM_OP_CMP_GT,  /* rA = rB >  rC */
  BIFROST_VM_OP_CMP_GE,  /* rA = rB >= rC */
  BIFROST_VM_OP_CMP_AND, /* rA = rB && rC */
  BIFROST_VM_OP_CMP_OR,  /* rA = rB || rC */
  BIFROST_VM_OP_NOT,     /* rA = !rBx     */

  // Control Flow
  BIFROST_VM_OP_CALL_FN,     /* call(local[rB]) (params-start = rA, num-args = rC) */
  BIFROST_VM_OP_JUMP,        /* ip += rsBx                                         */
  BIFROST_VM_OP_JUMP_IF,     /* if (rA) ip += rsBx                                 */
  BIFROST_VM_OP_JUMP_IF_NOT, /* if (!rA) ip += rsBx                                */
  BIFROST_VM_OP_RETURN,      /* breaks out of current function scope.              */

} bfInstructionOp;

/*!
   ///////////////////////////////////////////
   // 0     5         14        23       32 //
   // [ooooo|aaaaaaaaa|bbbbbbbbb|ccccccccc] //
   // [ooooo|aaaaaaaaa|bxbxbxbxbxbxbxbxbxb] //
   // [ooooo|aaaaaaaaa|sBxbxbxbxbxbxbxbxbx] //
   // opcode = 0       - 31                 //
   // rA     = 0       - 511                //
   // rB     = 0       - 511                //
   // rBx    = 0       - 262143             //
   // rsBx   = -131071 - 131072             //
   // rC     = 0       - 511                //
   ///////////////////////////////////////////
 */
typedef uint32_t bfInstruction;

#define BIFROST_INST_OP_MASK (bfInstruction)0x1F /* 31 in hex  */
#define BIFROST_INST_OP_OFFSET (bfInstruction)0
#define BIFROST_INST_RA_MASK (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RA_OFFSET (bfInstruction)5
#define BIFROST_INST_RB_MASK (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RB_OFFSET (bfInstruction)14
#define BIFROST_INST_RC_MASK (bfInstruction)0x1FF /* 511 in hex */
#define BIFROST_INST_RC_OFFSET (bfInstruction)23
#define BIFROST_INST_RBx_MASK (bfInstruction)0x3FFFF
#define BIFROST_INST_RBx_OFFSET (bfInstruction)14
#define BIFROST_INST_RsBx_MASK (bfInstruction)0x3FFFF
#define BIFROST_INST_RsBx_OFFSET (bfInstruction)14
#define BIFROST_INST_RsBx_MAX ((bfInstruction)BIFROST_INST_RsBx_MASK / 2)

#define BIFROST_INST_INVALID 0xFFFFFFFF

#define BIFROST_MAKE_INST_OP(op) \
  (bfInstruction)(op & BIFROST_INST_OP_MASK)

#define BIFROST_MAKE_INST_RC(c) \
  ((c & BIFROST_INST_RC_MASK) << BIFROST_INST_RC_OFFSET)

#define BIFROST_MAKE_INST_OP_ABC(op, a, b, c)               \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   ((b & BIFROST_INST_RB_MASK) << BIFROST_INST_RB_OFFSET) | \
   BIFROST_MAKE_INST_RC((c))

#define BIFROST_MAKE_INST_OP_ABx(op, a, bx)                 \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   ((bx & BIFROST_INST_RBx_MASK) << BIFROST_INST_RBx_OFFSET)

#define BIFROST_MAKE_INST_OP_AsBx(op, a, bx)                \
  BIFROST_MAKE_INST_OP(op) |                                \
   ((a & BIFROST_INST_RA_MASK) << BIFROST_INST_RA_OFFSET) | \
   (((bx + BIFROST_INST_RsBx_MAX) & BIFROST_INST_RsBx_MASK) << BIFROST_INST_RsBx_OFFSET)

/* NOTE(Shareef):
    This macro helps change / patch pieces of an instruction.

    inst : bfInstruction*;

    x    : can be equal to (capitalization matters!):
      > RA
      > RB
      > RC
      > RBx
      > RsBx
      > OP

    val  : integer (range depends on 'x').
 */
#define bfInstPatchX(inst, x, val) \
  *(inst) = (*(inst) & ~(BIFROST_INST_##x##_MASK << BIFROST_INST_##x##_OFFSET)) | BIFROST_MAKE_INST_##x(val)

#define bfVM_decodeOp(inst) ((uint8_t)((inst)&BIFROST_INST_OP_MASK))
#define bfVM_decodeRa(inst) ((uint32_t)(((inst) >> BIFROST_INST_RA_OFFSET) & BIFROST_INST_RA_MASK))
#define bfVM_decodeRb(inst) ((uint32_t)(((inst) >> BIFROST_INST_RB_OFFSET) & BIFROST_INST_RB_MASK))
#define bfVM_decodeRc(inst) ((uint32_t)(((inst) >> BIFROST_INST_RC_OFFSET) & BIFROST_INST_RC_MASK))
#define bfVM_decodeRBx(inst) ((uint32_t)(((inst) >> BIFROST_INST_RBx_OFFSET) & BIFROST_INST_RBx_MASK))
#define bfVM_decodeRsBx(inst) ((int32_t)(bfVM_decodeRBx((inst)) - BIFROST_INST_RsBx_MAX))
#if __cplusplus
}
#endif

#endif /* BIFROST_VM_INSTRUCTION_OP_H */
