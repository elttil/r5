#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

#define R_TYPE_DEF                                                             \
  u8 funct3 = (inst >> 12) & 0x7;                                              \
  u8 funct7 = (inst >> 25);                                                    \
  u8 rd = (inst >> 7) & 0x1F;                                                  \
  u8 rs1 = (inst >> 15) & 0x1F;                                                \
  u8 rs2 = (inst >> 20) & 0x1F;                                                \
  (void)rd;                                                                    \
  (void)rs1;                                                                   \
  (void)rs2;                                                                   \
  (void)funct3;                                                                \
  (void)funct7;

#define I_TYPE_DEF                                                             \
  u32 imm = (inst >> 20);                                                      \
  u8 funct3 = (inst >> 12) & 0x7;                                              \
  u8 rd = (inst >> 7) & 0x1F;                                                  \
  u8 rs1 = (inst >> 15) & 0x1F;                                                \
  (void)imm;                                                                   \
  (void)funct3;                                                                \
  (void)rd;                                                                    \
  (void)rs1;

#define U_TYPE_DEF                                                             \
  u32 imm = inst & ~(0x1000 - 1);                                              \
  u8 rd = (inst >> 7) & 0x1F;                                                  \
  (void)rd;                                                                    \
  (void)imm;

#define B_TYPE_DEF                                                             \
  u32 imm = ((inst & 0xf00) >> 7) | ((inst & 0x7e000000) >> 20) |              \
            ((inst & 0x80) << 4) | ((inst >> 31) << 12);                       \
  u8 rs1 = (inst >> 15) & 0x1F;                                                \
  u8 rs2 = (inst >> 20) & 0x1F;                                                \
  u8 funct3 = (inst >> 12) & 0x7;                                              \
  (void)rs1;                                                                   \
  (void)rs2;                                                                   \
  (void)funct3;                                                                \
  (void)imm;

#define J_TYPE_DEF                                                             \
  u32 imm = 0;                                                                 \
  imm |= (inst & (0x1 << 31));                                                 \
  imm |= (inst & (0xFF << 12)) << 11;                                          \
  imm |= (inst & (0x1 << 20)) << 2;                                            \
  imm |= (inst & (0x3FF << 21)) >> 9;                                          \
  imm = ((i32)imm) >> 11;                                                      \
  u8 rd = (inst >> 7) & 0x1F;                                                  \
  (void)rd;                                                                    \
  (void)imm;

#define S_TYPE_DEF                                                             \
  u32 imm = ((inst >> 7) & 0x1F) | ((inst >> 25) << 5);                        \
  u8 funct3 = (inst >> 12) & 0x7;                                              \
  u8 rs1 = (inst >> 15) & 0x1F;                                                \
  u8 rs2 = (inst >> 20) & 0x1F;                                                \
  (void)imm;                                                                   \
  (void)funct3;                                                                \
  (void)rs1;                                                                   \
  (void)rs2;

#define TODO_FUNC(_instruction_name)                                           \
  void inst_##_instruction_name(struct CPU *cpu, u32 inst) {                   \
    (void)cpu;                                                                 \
    (void)inst;                                                                \
    printf("Uninmplemented instruction: %s\n", #_instruction_name);            \
    assert(0);                                                                 \
  }

i32 sign_extend(u32 n, u8 len) {
  n &= ~(0xFFFFFFFF << (len + 1));
  if (n & (1 << len)) {
    n |= 0xFFFFFFFF << len;
  }
  return (i32)n;
}

u8 *ram;
struct CPU {
  u64 registers[32];
  u64 pc;
};

TODO_FUNC(slli)

void inst_addi(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  cpu->registers[rd] = cpu->registers[rs1] + b;
#ifdef DEBUG
  printf("%lx: addi x%d,x%d,%d\n", cpu->pc, rd, rs1, b);
#endif
}

void inst_slti(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  if ((i64)cpu->registers[rs1] < imm) {
    cpu->registers[rd] = 1;
  } else {
    cpu->registers[rd] = 0;
  }
}

void inst_sltiu(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  if ((u64)cpu->registers[rs1] < (u64)imm) {
    cpu->registers[rd] = 1;
  } else {
    cpu->registers[rd] = 0;
  }
}

void inst_andi(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i64 result = (i64)cpu->registers[rs1] & (i64)imm;
  cpu->registers[rd] = result;
}

void inst_ori(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  i64 result = cpu->registers[rs1] | b;
  cpu->registers[rd] = result;
}

void inst_xori(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  i64 result = cpu->registers[rs1] ^ b;
  cpu->registers[rd] = result;
}

void inst_srli(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  u64 to_be_shifted = cpu->registers[rs1];
  i64 shift_amount = imm & 0x1F;
  i64 right_shift_type = inst & (1 << 30);
  (void)right_shift_type; // FIXME
  u64 result = (u64)to_be_shifted >> shift_amount;
  cpu->registers[rd] = result;
}

void inst_srai(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i64 to_be_shifted = cpu->registers[rs1];
  i64 shift_amount = imm & 0x1F;
  i64 right_shift_type = inst & (1 << 30);
  (void)right_shift_type; // FIXME
  i64 result = to_be_shifted >> shift_amount;
  cpu->registers[rd] = result;
}

void inst_add(struct CPU *cpu, u32 inst) {
  R_TYPE_DEF
  cpu->registers[rd] = cpu->registers[rs1] + cpu->registers[rs2];
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

#define FUNCT3_BEQ 0x0
#define FUNCT3_BGE 0x5

#define FUNCT3_JALR 0x0
#define FUNCT3_ADDI 0x0
#define FUNCT3_SLLI 0x1
#define FUNCT3_SLTI 0x2
#define FUNCT3_SLTIU 0x3
#define FUNCT3_XORI 0x4
#define FUNCT3_SR 0x5
#define FUNCT3_ORI 0x6
#define FUNCT3_ANDI 0x7

void opcode_h13(struct CPU *cpu, u32 inst) {
  u8 funct3 = (inst >> 12) & 0x7;
  u8 funct7 = (inst >> 25) & 0x3F; // Only used for certain funct3
  switch (funct3) {
  case FUNCT3_ADDI:
    inst_addi(cpu, inst);
    break;
  case FUNCT3_SLTI:
    inst_slti(cpu, inst);
    break;
  case FUNCT3_SLTIU:
    inst_sltiu(cpu, inst);
    break;
  case FUNCT3_XORI:
    inst_xori(cpu, inst);
    break;
  case FUNCT3_ORI:
    inst_ori(cpu, inst);
    break;
  case FUNCT3_ANDI:
    inst_andi(cpu, inst);
    break;
  case FUNCT3_SLLI: {
    assert(0 == funct7);
    inst_slli(cpu, inst);
    break;
  }
  case FUNCT3_SR: {
    if (0 == funct7) {
      inst_srli(cpu, inst);
    } else {
      inst_srai(cpu, inst);
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

void inst_lui(struct CPU *cpu, u32 inst) {
  U_TYPE_DEF
  cpu->registers[rd] = imm;
#ifdef DEBUG
  printf("%lx: lui x%d,%d\n", cpu->pc, rd, imm >> 12);
#endif
}

void inst_jalr(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  u64 target_address = cpu->registers[rs1] + sign_extend(imm, 12);
  target_address &= ~(1); // Setting the least significant bit to zero

  cpu->registers[rd] = cpu->pc + 4;

  cpu->pc = target_address;
#ifdef DEBUG
  printf("%lx: jalr x%d,%d(x%d)\n", cpu->pc, rd, imm, rs1);
#endif
}

void opcode_h67(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  switch (funct3) {
  case FUNCT3_JALR:
    inst_jalr(cpu, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

void inst_sb(struct CPU *cpu, u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = cpu->registers[rs1] + b;
  *(u32 *)(ram + destination) = cpu->registers[rs2] & 0xFF;
}

void inst_sh(struct CPU *cpu, u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = cpu->registers[rs1] + b;
  *(u32 *)(ram + destination) = cpu->registers[rs2] & 0xFFFF;
}

void inst_sw(struct CPU *cpu, u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = cpu->registers[rs1] + b;
  *(u32 *)(ram + destination) = cpu->registers[rs2];
#ifdef DEBUG
  printf("%lx: sw x%d,%d(x%d)\n", cpu->pc, rs2, b, rs1);
#endif
}

void inst_sd(struct CPU *cpu, u32 inst) {
  S_TYPE_DEF

  i32 b = sign_extend(imm, 11);
  u64 destination = cpu->registers[rs1] + b;
  *(u64 *)(ram + destination) = cpu->registers[rs2];
#ifdef DEBUG
  printf("%lx: sd x%d,%d(x%d)\n", cpu->pc, rs2, b, rs1);
#endif
}

void opcode_h23(struct CPU *cpu, u32 inst) {
  S_TYPE_DEF
  switch (funct3) {
  case FUNCT3_SB:
    inst_sb(cpu, inst);
    break;
  case FUNCT3_SH:
    inst_sh(cpu, inst);
    break;
  case FUNCT3_SW:
    inst_sw(cpu, inst);
    break;
  case FUNCT3_SD:
    inst_sd(cpu, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

void inst_jal(struct CPU *cpu, u32 inst) {
  J_TYPE_DEF

  i64 offset = sign_extend(imm, 20);

  u64 jump_target_address = cpu->pc + offset;
  cpu->registers[rd] = cpu->pc + 4;
  cpu->pc = jump_target_address;
#ifdef DEBUG
  printf("%lx: jal x%d, %lx\n", cpu->pc, rd, jump_target_address);
#endif
}

void inst_beq(struct CPU *cpu, u32 inst) {
  B_TYPE_DEF
  if (cpu->registers[rs1] != cpu->registers[rs2])
    return;

  i64 offset = sign_extend(imm, 12);

  u64 jump_target_address = cpu->pc + offset;
  cpu->pc = jump_target_address;
}

void inst_bge(struct CPU *cpu, u32 inst) {
  B_TYPE_DEF

  i64 offset = sign_extend(imm, 12);
  u64 jump_target_address = cpu->pc + offset;
  if ((i64)cpu->registers[rs1] >= (i64)cpu->registers[rs2]) {
    cpu->pc = jump_target_address;
  }
#ifdef DEBUG
  printf("%lx: bge x%d,x%d,%lx\n", cpu->pc, rs1, rs2, jump_target_address);
#endif
}

void opcode_h63(struct CPU *cpu, u32 inst) {
  B_TYPE_DEF
  switch (funct3) {
  case FUNCT3_BEQ:
    inst_beq(cpu, inst);
    break;
  case FUNCT3_BGE:
    inst_bge(cpu, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

void inst_lw(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 location = cpu->registers[rs1] + b;
  u32 value = *(u32 *)(ram + location);
  cpu->registers[rd] = value;
#ifdef DEBUG
  printf("%lx: lw x%d, %d(x%d)\n", cpu->pc, rd, b, rs1);
#endif
}

void inst_ld(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  i32 b = sign_extend(imm, 11);
  u64 location = cpu->registers[rs1] + b;
  u64 value = *(u64 *)(ram + location);
  cpu->registers[rd] = value;
#ifdef DEBUG
  printf("%lx: lw x%d, %d(x%d)\n", cpu->pc, rd, b, rs1);
#endif
}

void opcode_h03(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF
  switch (funct3) {
  case FUNCT3_LW:
    inst_lw(cpu, inst);
    break;
  case FUNCT3_LD:
    inst_ld(cpu, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

#define FUNCT3_ADDW 0x0
#define FUNCT7_ADDW 0x0

void inst_addw(struct CPU *cpu, u32 inst) {
  R_TYPE_DEF

  i32 a = (i32)cpu->registers[rs1];
  i32 b = (i32)cpu->registers[rs2];
  cpu->registers[rd] = a + b;
}

void opcode_h3B(struct CPU *cpu, u32 inst) {
  R_TYPE_DEF
  switch (funct3) {
  case FUNCT3_ADDW:
    if (FUNCT7_ADDW == funct7) {
      inst_addw(cpu, inst);
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

void inst_addiw(struct CPU *cpu, u32 inst) {
  I_TYPE_DEF

  i32 a = (i32)cpu->registers[rs1];
  i32 b = sign_extend(imm, 11);
  cpu->registers[rd] = a + b;
}

void opcode_h1B(struct CPU *cpu, u32 inst) {
  R_TYPE_DEF
  switch (funct3) {
  case FUNCT3_ADDIW:
    inst_addiw(cpu, inst);
    break;
  default:
    printf("Unknown funct3: %x in opcode: %x\n", funct3, inst & 0x7F);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

void perform_instruction(struct CPU *cpu, u32 inst) {
  u8 opcode = inst & 0x7F;
  switch (opcode) {
  case 0x3:
    opcode_h03(cpu, inst);
    break;
  case 0x13:
    opcode_h13(cpu, inst);
    break;
  case 0x1B:
    opcode_h1B(cpu, inst);
    break;
  case 0x23:
    opcode_h23(cpu, inst);
    break;
  case 0x33:
    inst_add(cpu, inst);
    break;
  case 0x37:
    inst_lui(cpu, inst);
    break;
  case 0x3B:
    opcode_h3B(cpu, inst);
    break;
  case 0x63:
    opcode_h63(cpu, inst);
    break;
  case 0x67:
    opcode_h67(cpu, inst);
    break;
  case 0x6F:
    inst_jal(cpu, inst);
    break;
  default:
    printf("Unknown opcode: %x\n", opcode);
    cpu_dump_state(cpu);
    assert(0);
    break;
  }
}

void cpu_loop(struct CPU *cpu) {
  for (;;) {
    u32 inst = *(u32 *)(ram + cpu->pc);
    u64 old_pc = cpu->pc;
    cpu->registers[0] = 0;
    perform_instruction(cpu, inst);
    if (cpu->pc != old_pc)
      continue;
    cpu->pc += sizeof(u32);
  }
}

bool load_file(const char *file) {
  int fd = open(file, O_RDONLY);
  if (-1 == fd) {
    perror("open");
    return false;
  }
  int rc = read(fd, ram, 4096);
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

int main(void) {
  struct CPU cpu;
  cpu.pc = 0;
  ram = calloc(100, 4096);
  for (int i = 0; i < 32; i++) {
    cpu.registers[i] = 0;
  }
  cpu.pc = 0xa4;

  if (!load_file("./fib-example/flat")) {
    return 1;
  }

  cpu_loop(&cpu);
  return 0;
}