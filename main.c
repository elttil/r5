#include "mmu.h"
#include "types.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define RD_REGISTER                                                            \
  const u8 num_rd = (inst >> 7) & 0x1F;                                        \
  u64 zero_tmp;                                                                \
  u64 *_rd;                                                                    \
  if (0 == num_rd) {                                                           \
    _rd = &zero_tmp;                                                           \
  } else {                                                                     \
    _rd = &cpu->registers[num_rd];                                             \
  }                                                                            \
  u64 *const rd = _rd;                                                         \
  (void)zero_tmp;                                                              \
  (void)rd;

#define RS1_REGISTER                                                           \
  const u8 num_rs1 = (inst >> 15) & 0x1F;                                      \
  const u64 *const rs1 = &cpu->registers[num_rs1];                             \
  (void)rs1;

#define RS2_REGISTER                                                           \
  const u8 num_rs2 = (inst >> 20) & 0x1F;                                      \
  const u64 *const rs2 = &cpu->registers[num_rs2];                             \
  (void)rs2;

#define R_TYPE_DEF                                                             \
  const u8 funct3 = (inst >> 12) & 0x7;                                        \
  const u8 funct7 = (inst >> 25);                                              \
  RD_REGISTER;                                                                 \
  RS1_REGISTER;                                                                \
  RS2_REGISTER;                                                                \
  (void)funct3;                                                                \
  (void)funct7;

#define I_TYPE_DEF                                                             \
  const u32 imm = (inst >> 20);                                                \
  const u8 funct3 = (inst >> 12) & 0x7;                                        \
  RD_REGISTER;                                                                 \
  RS1_REGISTER;                                                                \
  (void)imm;                                                                   \
  (void)funct3;

#define U_TYPE_DEF                                                             \
  const u32 imm = inst & ~(0x1000 - 1);                                        \
  RD_REGISTER;                                                                 \
  (void)imm;

#define B_TYPE_DEF                                                             \
  const u32 imm = ((inst & 0xf00) >> 7) | ((inst & 0x7e000000) >> 20) |        \
                  ((inst & 0x80) << 4) | ((inst >> 31) << 12);                 \
  const u8 funct3 = (inst >> 12) & 0x7;                                        \
  RS1_REGISTER;                                                                \
  RS2_REGISTER;                                                                \
  (void)funct3;                                                                \
  (void)imm;

#define J_TYPE_DEF                                                             \
  u32 _dont_use = 0;                                                           \
  _dont_use |= (inst & (0x1 << 31));                                           \
  _dont_use |= (inst & (0xFF << 12)) << 11;                                    \
  _dont_use |= (inst & (0x1 << 20)) << 2;                                      \
  _dont_use |= (inst & (0x3FF << 21)) >> 9;                                    \
  _dont_use = ((i32)_dont_use) >> 11;                                          \
  const u32 imm = _dont_use;                                                   \
  RD_REGISTER;                                                                 \
  (void)imm;

#define S_TYPE_DEF                                                             \
  const u32 imm = ((inst >> 7) & 0x1F) | ((inst >> 25) << 5);                  \
  const u8 funct3 = (inst >> 12) & 0x7;                                        \
  RS1_REGISTER;                                                                \
  RS2_REGISTER;                                                                \
  (void)imm;                                                                   \
  (void)funct3;

i32 sign_extend(u32 n, u8 len) {
  n &= ~(0xFFFFFFFF << (len + 1));
  if (n & (1 << len)) {
    n |= 0xFFFFFFFF << len;
  }
  return (i32)n;
}

struct CPU {
  u64 registers[32];
  u64 pc;
  bool did_branch;
};

static void inst_slli(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  u64 to_shift = *rs1;
  u8 shift_amount = imm & 0x3F;
  u64 result = to_shift << shift_amount;
  *rd = result;
#ifdef DEBUG
  printf("%lx: slli x%d,x%d,%d\n", cpu->pc, rd, rs1, shift_amount);
#endif
}

static void inst_addi(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  *rd = (i64)*rs1 + b;
#ifdef DEBUG
  printf("%lx: addi x%d,x%d,%d\n", cpu->pc, rd, rs1, b);
#endif
}

static void inst_slti(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  if ((i64)*rs1 < (i64)sign_extend(imm, 11)) {
    *rd = 1;
  } else {
    *rd = 0;
  }
}

static void inst_sltiu(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  if ((u64)*rs1 < (u64)imm) {
    *rd = 1;
  } else {
    *rd = 0;
  }
}

static void inst_andi(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 result = (i64)*rs1 & (i64)b;
  *rd = result;
}

static void inst_ori(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  i64 result = (i64)*rs1 | b;
  *rd = result;
}

static void inst_xori(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  i64 result = (i64)*rs1 ^ (i64)b;
  *rd = result;
}

static void inst_srli(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  u64 to_be_shifted = *rs1;
  u8 shift_amount = imm & 0x3F;
  u64 result = to_be_shifted >> shift_amount;
  *rd = result;
}

static void inst_srai(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i64 to_be_shifted = *rs1;
  u8 shift_amount = imm & 0x3F;
  i64 result = to_be_shifted >> shift_amount;
  *rd = result;
}

static void inst_add(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 + *rs2;
}

static void inst_sltu(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  if (*rs1 < *rs2) {
    *rd = 1;
  } else {
    *rd = 0;
  }
}

static void inst_and(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 & *rs2;
}

static void inst_or(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 | *rs2;
}

static void inst_xor(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 ^ *rs2;
}

void cpu_dump_state(struct CPU *cpu) {
  printf("CPU dump:\n");
  for (int i = 0; i < 32; i++) {
    printf("reg %d: %ld\n", i, cpu->registers[i]);
  }
}

#define FUNCT3_SB 0x0
#define FUNCT3_SH 0x1
#define FUNCT3_SW 0x2
#define FUNCT3_SD 0x3

#define FUNCT3_LW 0x2
#define FUNCT3_LD 0x3
#define FUNCT3_LBU 0x4

#define FUNCT3_BEQ 0x0
#define FUNCT3_BNE 0x1
#define FUNCT3_BGE 0x5
#define FUNCT3_BLTU 0x6
#define FUNCT3_BGEU 0x7

#define FUNCT3_JALR 0x0
#define FUNCT3_ADDI 0x0
#define FUNCT3_SLLI 0x1
#define FUNCT3_SLTI 0x2
#define FUNCT3_SLTIU 0x3
#define FUNCT3_XORI 0x4
#define FUNCT3_SR 0x5
#define FUNCT3_ORI 0x6
#define FUNCT3_ANDI 0x7

#define FUNCT3_SLLIW 0x1
#define FUNCT3_SRW 0x5

#define FUNCT3_ADD 0x0
#define FUNCT3_SLTU 0x3
#define FUNCT3_AND 0x7
#define FUNCT3_OR 0x6
#define FUNCT3_XOR 0x4

static void opcode_h13(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  u8 funct3 = (inst >> 12) & 0x7;
  u8 funct7 = (inst >> 26 /* shamt is sligthly bigger for 64 bit */) &
              0x3F; // Only used for certain funct3
  switch (funct3) {
  case FUNCT3_ADDI:
    inst_addi(cpu, mem, inst);
    break;
  case FUNCT3_SLTI:
    inst_slti(cpu, mem, inst);
    break;
  case FUNCT3_SLTIU:
    inst_sltiu(cpu, mem, inst);
    break;
  case FUNCT3_XORI:
    inst_xori(cpu, mem, inst);
    break;
  case FUNCT3_ORI:
    inst_ori(cpu, mem, inst);
    break;
  case FUNCT3_ANDI:
    inst_andi(cpu, mem, inst);
    break;
  case FUNCT3_SLLI: {
    assert(0 == funct7);
    inst_slli(cpu, mem, inst);
    break;
  }
  case FUNCT3_SR: {
    if (0 == funct7) {
      inst_srli(cpu, mem, inst);
    } else {
      inst_srai(cpu, mem, inst);
    }
    break;
  }
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void inst_lui(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  U_TYPE_DEF
  *rd = imm;
#ifdef DEBUG
  printf("%lx: lui x%d,%d\n", cpu->pc, rd, imm >> 12);
#endif
}

static void inst_jalr(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  u64 target_address = *rs1 + sign_extend(imm, 12);
  target_address &= ~(1); // Setting the least significant bit to zero

  *rd = cpu->pc + 4;

#ifdef DEBUG
  printf("%lx: jalr x%d,%d(x%d)\n", cpu->pc, rd, imm, rs1);
#endif
  cpu->pc = target_address;
  cpu->did_branch = true;
}

static void opcode_h67(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  I_TYPE_DEF
  switch (funct3) {
  case FUNCT3_JALR:
    inst_jalr(cpu, mem, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void inst_sb(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = *rs1 + b;
  u8 tmp_value = *rs2;
  memory_write(mem, destination, &tmp_value, sizeof(tmp_value));
}

static void inst_sh(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = *rs1 + b;
  u16 tmp_value = *rs2;
  memory_write(mem, destination, &tmp_value, sizeof(tmp_value));
}

static void inst_sw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = *rs1 + (i64)b;
  u32 value = *rs2;
  memory_write(mem, destination, &value, sizeof(value));
#ifdef DEBUG
  printf("%lx: sw x%d,%d(x%d)\n", cpu->pc, rs2, b, rs1);
#endif
}

static void inst_sd(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = *rs1 + b;
  u64 value = *rs2;
  memory_write(mem, destination, &value, sizeof(value));
#ifdef DEBUG
  printf("%lx: sd x%d,%d(x%d)\n", cpu->pc, rs2, b, rs1);
#endif
}

static void opcode_h23(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  S_TYPE_DEF
  switch (funct3) {
  case FUNCT3_SB:
    inst_sb(cpu, mem, inst);
    break;
  case FUNCT3_SH:
    inst_sh(cpu, mem, inst);
    break;
  case FUNCT3_SW:
    inst_sw(cpu, mem, inst);
    break;
  case FUNCT3_SD:
    inst_sd(cpu, mem, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void inst_jal(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  J_TYPE_DEF

  i64 offset = sign_extend(imm, 20);

  u64 jump_target_address = cpu->pc + offset;
  *rd = cpu->pc + 4;
#ifdef DEBUG
  printf("%lx: jal x%d, %lx\n", cpu->pc, rd, jump_target_address);
#endif
  cpu->pc = jump_target_address;
  cpu->did_branch = true;
}

static void inst_beq(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  B_TYPE_DEF
  if (*rs1 != *rs2)
    return;

  i64 offset = sign_extend(imm, 12);

  u64 jump_target_address = cpu->pc + offset;
  cpu->pc = jump_target_address;
  cpu->did_branch = true;
}

static void inst_bge(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  B_TYPE_DEF

  i64 offset = sign_extend(imm, 12);
  u64 jump_target_address = cpu->pc + offset;
#ifdef DEBUG
  printf("%lx: bge x%d,x%d,%lx\n", cpu->pc, rs1, rs2, jump_target_address);
#endif
  if ((i64)*rs1 >= (i64)*rs2) {
    cpu->pc = jump_target_address;
    cpu->did_branch = true;
  }
}

static void inst_bgeu(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  B_TYPE_DEF

  i64 offset = sign_extend(imm, 12);
  u64 jump_target_address = cpu->pc + offset;
#ifdef DEBUG
  printf("%lx: bge x%d,x%d,%lx\n", cpu->pc, rs1, rs2, jump_target_address);
#endif
  if (*rs1 >= *rs2) {
    cpu->pc = jump_target_address;
    cpu->did_branch = true;
  }
}

static void inst_bne(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  B_TYPE_DEF

  i64 offset = sign_extend(imm, 12);
  u64 jump_target_address = cpu->pc + offset;
#ifdef DEBUG
  printf("%lx: bne x%d,x%d,%lx\n", cpu->pc, rs1, rs2, jump_target_address);
#endif
  if ((i64)*rs1 != (i64)*rs2) {
    cpu->pc = jump_target_address;
    cpu->did_branch = true;
  }
}

static void inst_bltu(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  B_TYPE_DEF

  i64 offset = sign_extend(imm, 12);
  u64 jump_target_address = cpu->pc + offset;
#ifdef DEBUG
  printf("%lx: bltu x%d,x%d,%lx\n", cpu->pc, rs1, rs2, jump_target_address);
#endif
  if (*rs1 < *rs2) {
    cpu->pc = jump_target_address;
    cpu->did_branch = true;
  }
}

static void opcode_h63(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  B_TYPE_DEF
  switch (funct3) {
  case FUNCT3_BNE:
    inst_bne(cpu, mem, inst);
    break;
  case FUNCT3_BEQ:
    inst_beq(cpu, mem, inst);
    break;
  case FUNCT3_BGE:
    inst_bge(cpu, mem, inst);
    break;
  case FUNCT3_BLTU:
    inst_bltu(cpu, mem, inst);
    break;
  case FUNCT3_BGEU:
    inst_bgeu(cpu, mem, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void inst_lw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 location = *rs1 + (i64)b;
  i32 value;
  memory_read(mem, location, &value, sizeof(value));
  *rd = (i64)value;
#ifdef DEBUG
  printf("%lx: lw x%d, %d(x%d)\n", cpu->pc, rd, b, rs1);
#endif
}

static void inst_ld(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 location = *rs1 + b;
  i64 value;
  memory_read(mem, location, &value, sizeof(i64));
  *rd = value;
#ifdef DEBUG
  printf("%lx: ld x%d, %d(x%d)\n", cpu->pc, rd, b, rs1);
#endif
}

static void inst_lbu(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 location = *rs1 + b;
  u8 value;
  memory_read(mem, location, &value, sizeof(u8));
  *rd = value;
#ifdef DEBUG
  printf("%lx: lbu x%d, %d(x%d)\n", cpu->pc, rd, b, rs1);
#endif
}

static void opcode_h03(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  I_TYPE_DEF
  switch (funct3) {
  case FUNCT3_LW:
    inst_lw(cpu, mem, inst);
    break;
  case FUNCT3_LD:
    inst_ld(cpu, mem, inst);
    break;
  case FUNCT3_LBU:
    inst_lbu(cpu, mem, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

#define FUNCT3_ADDW 0x0
#define FUNCT3_SLLW 0x1

#define FUNCT3_SRLW_SRAW 0x5

#define FUNCT7_ADDW 0x0
#define FUNCT7_SUBW (0x1 << 5)

static void inst_addw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 + *rs2;
}

static void inst_subw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  *rd = *rs1 - *rs2;
}

static void inst_sllw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF

  u64 to_shift = *rs1;
  u8 shift_amount = *rs2 & 0x1F;
  i32 result = (to_shift << shift_amount);
  *rd = (i64)result;
}

static void inst_srlw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  u32 to_be_shifted = *rs1;
  u8 shift_amount = *rs2 & 0x1F;
  i32 result = to_be_shifted >> shift_amount;
  *rd = (i64)result;
}

static void inst_sraw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  R_TYPE_DEF
  i32 to_be_shifted = *rs1;
  u8 shift_amount = *rs2 & 0x1F;
  i32 result = to_be_shifted >> shift_amount;
  *rd = (i64)result;
}

static void opcode_h3B(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  R_TYPE_DEF
  switch (funct3) {
  case FUNCT3_SLLW:
    if (0 == funct7) {
      inst_sllw(cpu, mem, inst);
    }
    break;
  case FUNCT3_ADDW:
    if (FUNCT7_ADDW == funct7) {
      inst_addw(cpu, mem, inst);
    } else if (FUNCT7_SUBW == funct7) {
      inst_subw(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  case FUNCT3_SRLW_SRAW:
    if (0 == funct7) {
      inst_srlw(cpu, mem, inst);
    }
    break;
    if ((1 << 5) == funct7) {
      inst_sraw(cpu, mem, inst);
    }
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

#define FUNCT3_ADDIW 0x0

static void inst_addiw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF

  i64 a = *rs1;
  i64 b = sign_extend(imm, 11);
  *rd = a + b;
}

uint64_t kUpper32bitMask = 0xFFFFFFFF00000000;
u64 Sext32bit(u64 data32bit) {
  if ((data32bit >> 31) & 1) {
    return data32bit | kUpper32bitMask;
  } else {
    return data32bit & (~kUpper32bitMask);
  }
}

static void inst_srliw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  u64 to_be_shifted = *rs1 & 0xFFFFFFFF;
  u8 shift_amount = imm & 0x3F;
  u64 result = to_be_shifted >> shift_amount;
  result = Sext32bit(result);
  *rd = result;
}

static void inst_sraiw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  i64 to_be_shifted = (i64)*rs1;
  u8 shift_amount = imm & 0x1F;
  i64 result = to_be_shifted >> shift_amount;
  *rd = (i64)result;
}

static void inst_slliw(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  (void)mem;
  I_TYPE_DEF
  u32 to_shift = *rs1;
  u8 shift_amount = imm & 0x1F;
  u32 result = to_shift << shift_amount;
  *rd = result;
#ifdef DEBUG
  printf("%lx: slli x%d,x%d,%d\n", cpu->pc, rd, rs1, shift_amount);
#endif
}

static void opcode_h1B(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  u8 funct3 = (inst >> 12) & 0x7;
  u8 funct7 = (inst >> 25) & 0x3F; // Only used for certain funct3
  switch (funct3) {
  case FUNCT3_ADDIW:
    inst_addiw(cpu, mem, inst);
    break;
  case FUNCT3_SLLIW:
    inst_slliw(cpu, mem, inst);
    break;
  case FUNCT3_SRW: {
    if (0 == funct7) {
      inst_srliw(cpu, mem, inst);
    } else {
      inst_sraiw(cpu, mem, inst);
    }
    break;
  }
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void opcode_h33(struct CPU *cpu, struct Memory *mem, const u32 inst) {
  u8 funct3 = (inst >> 12) & 0x7;
  u8 funct7 = (inst >> 25) & 0x3F; // Only used for certain funct3
  switch (funct3) {
  case FUNCT3_ADD:
    if (0 == funct7) {
      inst_add(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  case FUNCT3_SLTU:
    if (0 == funct7) {
      inst_sltu(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  case FUNCT3_XOR:
    if (0 == funct7) {
      inst_xor(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  case FUNCT3_AND:
    if (0 == funct7) {
      inst_and(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  case FUNCT3_OR:
    if (0 == funct7) {
      inst_or(cpu, mem, inst);
    } else {
      assert(0);
    }
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void perform_instruction(struct CPU *cpu, struct Memory *mem,
                                const u32 inst) {
  cpu->did_branch = false;
  u8 opcode = inst & 0x7F;
  switch (opcode) {
  case 0x3:
    opcode_h03(cpu, mem, inst);
    break;
  case 0x13:
    opcode_h13(cpu, mem, inst);
    break;
  case 0x1B:
    opcode_h1B(cpu, mem, inst);
    break;
  case 0x23:
    opcode_h23(cpu, mem, inst);
    break;
  case 0x33:
    opcode_h33(cpu, mem, inst);
    break;
  case 0x37:
    inst_lui(cpu, mem, inst);
    break;
  case 0x3B:
    opcode_h3B(cpu, mem, inst);
    break;
  case 0x63:
    opcode_h63(cpu, mem, inst);
    break;
  case 0x67:
    opcode_h67(cpu, mem, inst);
    break;
  case 0x6F:
    inst_jal(cpu, mem, inst);
    break;
  default:
    printf("Unknown opcode: %x\n", opcode);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

static void cpu_loop(struct CPU *cpu, struct Memory *mem) {
  for (;;) {
    u32 inst;
    memory_read(mem, cpu->pc, &inst, sizeof(u32));
    perform_instruction(cpu, mem, inst);
    if (cpu->did_branch)
      continue;
    cpu->pc += sizeof(u32);
  }
}

bool load_file(const char *file, struct Memory *mem, u64 offset) {
  int fd = open(file, O_RDONLY);
  if (-1 == fd) {
    perror("open");
    return false;
  }
  int rc = read(fd, mem->ram + offset, 8192);
  if (-1 == rc) {
    perror("read");
    return false;
  }
  if (-1 == close(fd)) {
    perror("close");
    return false;
  }
  return true;
}

void cpu_init(struct CPU *cpu, u64 pc) {
  for (int i = 0; i < 32; i++) {
    cpu->registers[i] = 0;
  }
  cpu->did_branch = false;
  cpu->pc = pc;
}

int main(void) {
  struct CPU cpu;
  struct Memory mem;
  if (!ram_init(&mem, 1048576)) {
    return 1;
  }
  cpu_init(&cpu, 0x1000);

  if (!load_file("./fib-example/flat", &mem, 0x1000)) {
    return 1;
  }

  cpu_loop(&cpu, &mem);
  return 0;
}
