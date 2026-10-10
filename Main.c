#define SDL_MAIN_HANDLED  // Desativa o gerenciamento automático do WinMain da SDL2

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define SCREEN_WIDTH 224
#define SCREEN_HEIGHT 256

typedef struct {
  uint8_t z:1;
  uint8_t s:1;
  uint8_t p:1;
  uint8_t cy:1;
  uint8_t ac:1;
  uint8_t pad:3;
} ConditionCodes;

typedef struct {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;

    uint16_t sp; //Stack counter 16bit
    uint16_t pc; //Program counter 16bit

    uint8_t *memory; //Pointer to ram memory 64kb

    ConditionCodes cc; //flags
    uint8_t int_enable;

    // Hardware de Shift do Space Invaders
    uint16_t shift_register;
    uint8_t shift_offset;

    uint8_t port1; // Estado dos botões da porta 1

} State8080;

State8080* Init8080(void) {
  State8080* state = calloc(1, sizeof(State8080)); //Allocate memory in the State8080 and set it to 0
  state -> memory = malloc(65536); //65Kb ram memory
  state -> pc = 0;
  state->port1 = 0x08; // Bit 3 sempre ligado no hardware real
  return state;
}

int ReadFileToMemory(State8080* state, const char* filename, uint32_t mem_offset){
  FILE *f = fopen(filename, "rb");
  if (f == NULL){
    printf("ERROR: could not open the file '%s'\n", filename);
  }

  fseek(f, 0L, SEEK_END);
  int fsize = ftell(f);
  fseek(f, 0L, SEEK_SET);

  fread(&state->memory[mem_offset], fsize, 1, f);

  fclose(f);
  printf("LOADED '%s' (%d bytes) on address 0x%04X succefully\n", filename, fsize, mem_offset);
  return 1;
}

int Disassemble8080Op(unsigned char *codebuffer, int pc) {
    unsigned char *code = &codebuffer[pc];
    int opbytes = 1;
    printf("%04x ", pc);

    switch (*code) {
        case 0x00: printf("NOP"); break;
        case 0x01: printf("LXI    B,#$0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x02: printf("STAX   B"); break;
        case 0x03: printf("INX    B"); break;
        case 0x04: printf("INR    B"); break;
        case 0x05: printf("DCR    B"); break;
        case 0x06: printf("MVI    B,#$0x%02x", code[1]); opbytes = 2; break;
        case 0x07: printf("RLC"); break;
        case 0x0f: printf("RRC"); break;
        case 0x11: printf("LXI    D,#$0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x13: printf("INX    D"); break;
        case 0x19: printf("DAD    D"); break;
        case 0x1a: printf("LDAX   D"); break;
        case 0x21: printf("LXI    H,#$0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x23: printf("INX    H"); break;
        case 0x26: printf("MVI    H,#$0x%02x", code[1]); opbytes = 2; break;
        case 0x29: printf("DAD    H"); break;
        case 0x31: printf("LXI    SP,#$0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x32: printf("STA    $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x35: printf("DCR    M"); break;
        case 0x36: printf("MVI    M,#$0x%02x", code[1]); opbytes = 2; break;
        case 0x3a: printf("LDA    $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0x3e: printf("MVI    A,#$0x%02x", code[1]); opbytes = 2; break;
        case 0x67: printf("MOV    H,A"); break;
        case 0xa7: printf("ANA    A"); break;
        case 0xc2: printf("JNZ    $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0xc3: printf("JMP    $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0xc5: printf("PUSH   B"); break;
        case 0xc9: printf("RET"); break;
        case 0xca: printf("JZ     $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0xcd: printf("CALL   $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0xd3: printf("OUT    #$0x%02x", code[1]); opbytes = 2; break;
        case 0xd5: printf("PUSH   D"); break;
        case 0xda: printf("JC     $0x%02x%02x", code[2], code[1]); opbytes = 3; break;
        case 0xdb: printf("IN     #$0x%02x", code[1]); opbytes = 2; break;
        case 0xe5: printf("PUSH   H"); break;
        case 0xe6: printf("ANI    #$0x%02x", code[1]); opbytes = 2; break;
        case 0xf5: printf("PUSH   PSW"); break;

        default: printf("OPCODE NAO TRATADO: 0x%02x", *code); break;
    }

    printf("\n");
    return opbytes;
}

void RenderScreen(State8080* state, SDL_Renderer* renderer, SDL_Texture* texture, uint32_t* pixels) {
    // VRAM começa em 0x2400
    uint16_t vram_start = 0x2400;

    for (int col = 0; col < SCREEN_WIDTH; col++) {
        for (int row = 0; row < SCREEN_HEIGHT; row += 8) {
            // Cada byte na VRAM contém 8 pixels na vertical
            uint8_t byte = state->memory[vram_start + (col * 32) + (row / 8)];

            for (int bit = 0; bit < 8; bit++) {
                int x = col;
                int y = SCREEN_HEIGHT - 1 - (row + bit);

                // Verifica se o bit está ativo
                uint32_t color = (byte & (1 << bit)) ? 0xFFFFFFFF : 0x000000FF;

                // Salva no buffer de pixels da SDL
                pixels[y * SCREEN_WIDTH + x] = color;
            }
        }
    }

    // Atualiza a textura com os novos pixels e desenha na tela
    SDL_UpdateTexture(texture, NULL, pixels, SCREEN_WIDTH * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void Emulate8080p(State8080* state){
unsigned char *code = &state->memory[state->pc];

    switch (*code) {
        // --- NOP ---
        // --- NOPs (Oficiais e Não-Oficiais) ---
        case 0x00: 
        case 0x08: 
        case 0x10: 
        case 0x18: 
        case 0x20: 
        case 0x28: 
        case 0x30: 
        case 0x38: 
            state->pc += 1; 
            break;
        // --- ADD E (0x83) ---
        case 0x83: {
            uint16_t res = state->a + state->e;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        // --- Entrada de Dados (IN) ---
        case 0xdb: { // IN port
            uint8_t port = code[1];
            if (port == 1) {
                state->a = state->port1;
            } else if (port == 2) {
                state->a = 0x00;
            } else if (port == 3) {
                uint16_t res = state->shift_register >> (8 - state->shift_offset);
                state->a = res & 0xff;
            } else {
                state->a = 0x00;
            }
            state->pc += 2;
            break;
        }
        // --- ADD C (0x81) ---
        case 0x81: {
            uint16_t res = state->a + state->c;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }

        // --- ADD L (0x85) ---
        case 0x85: {
            uint16_t res = state->a + state->l;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        // --- Chamada Condicional se Paridade Par (CPE) ---
        case 0xec: { // CPE adr
            if (state->cc.p == 1) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }

        // --- Chamada Condicional se Paridade Ímpar (CPO) ---
        case 0xe4: { // CPO adr
            if (state->cc.p == 0) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }

        // --- Cargas Imediatas de 16 bits (LXI) ---
        case 0x01: state->c = code[1]; state->b = code[2]; state->pc += 3; break; // LXI B
        case 0x11: state->e = code[1]; state->d = code[2]; state->pc += 3; break; // LXI D
        case 0x21: state->l = code[1]; state->h = code[2]; state->pc += 3; break; // LXI H
        case 0x31: state->sp = (code[2] << 8) | code[1]; state->pc += 3; break;   // LXI SP

        // --- Incrementos de 16 bits (INX) ---
        case 0x03: { uint16_t res = ((state->b << 8) | state->c) + 1; state->b = res >> 8; state->c = res; state->pc += 1; break; }
        case 0x13: { uint16_t res = ((state->d << 8) | state->e) + 1; state->d = res >> 8; state->e = res; state->pc += 1; break; }
        case 0x23: { uint16_t res = ((state->h << 8) | state->l) + 1; state->h = res >> 8; state->l = res; state->pc += 1; break; } // INX H (0x23)
        case 0x33: state->sp++; state->pc += 1; break;

        // --- Leitura e Escrita Indireta (LDAX / STAX) ---
        case 0x02: state->memory[(state->b << 8) | state->c] = state->a; state->pc += 1; break; // STAX B
        case 0x12: state->memory[(state->d << 8) | state->e] = state->a; state->pc += 1; break; // STAX D
        case 0x0a: state->a = state->memory[(state->b << 8) | state->c]; state->pc += 1; break; // LDAX B
        case 0x1a: state->a = state->memory[(state->d << 8) | state->e]; state->pc += 1; break; // LDAX D (0x1A)

        // --- Movimentação com Registrador e Memória (MVI / MOV) ---
        case 0x06: state->b = code[1]; state->pc += 2; break; // MVI B
        case 0x0e: state->c = code[1]; state->pc += 2; break; // MVI C
        case 0x16: state->d = code[1]; state->pc += 2; break; // MVI D
        case 0x1e: state->e = code[1]; state->pc += 2; break; // MVI E
        case 0x26: state->h = code[1]; state->pc += 2; break; // MVI H
        case 0x2e: state->l = code[1]; state->pc += 2; break; // MVI L
        case 0x36: state->memory[(state->h << 8) | state->l] = code[1]; state->pc += 2; break; // MVI M
        case 0x3e: state->a = code[1]; state->pc += 2; break; // MVI A

        case 0x77: state->memory[(state->h << 8) | state->l] = state->a; state->pc += 1; break; // MOV M, A
        case 0x7e: state->a = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV A, M

        // --- Leitura e Escrita Direta na RAM (STA / LDA) ---
        case 0x32: state->memory[(code[2] << 8) | code[1]] = state->a; state->pc += 3; break; // STA
        case 0x3a: state->a = state->memory[(code[2] << 8) | code[1]]; state->pc += 3; break; // LDA

        // --- Controle de Fluxo (JMP / CALL / RET) ---
        case 0xc3: state->pc = (code[2] << 8) | code[1]; break; // JMP
        case 0xc2: if (state->cc.z == 0) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JNZ
        case 0xca: if (state->cc.z == 1) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JZ
        case 0xd2: if (state->cc.cy == 0) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JNC
        case 0xda: if (state->cc.cy == 1) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JC

        case 0xcd: { // CALL
            uint16_t ret = state->pc + 3;
            state->memory[state->sp - 1] = (ret >> 8) & 0xff;
            state->memory[state->sp - 2] = ret & 0xff;
            state->sp -= 2;
            state->pc = (code[2] << 8) | code[1];
            break;
        }
        case 0xc9: { // RET
            state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
            state->sp += 2;
            break;
        }

        // --- Pilha (PUSH / POP) ---
        case 0xc5: // PUSH B
            state->memory[state->sp - 1] = state->b;
            state->memory[state->sp - 2] = state->c;
            state->sp -= 2;
            state->pc += 1;
            break;
        case 0xd5: // PUSH D
            state->memory[state->sp - 1] = state->d;
            state->memory[state->sp - 2] = state->e;
            state->sp -= 2;
            state->pc += 1;
            break;
        case 0xe5: // PUSH H
            state->memory[state->sp - 1] = state->h;
            state->memory[state->sp - 2] = state->l;
            state->sp -= 2;
            state->pc += 1;
            break;

        case 0xc1: // POP B
            state->c = state->memory[state->sp];
            state->b = state->memory[state->sp + 1];
            state->sp += 2;
            state->pc += 1;
            break;
        case 0xd1: // POP D
            state->e = state->memory[state->sp];
            state->d = state->memory[state->sp + 1];
            state->sp += 2;
            state->pc += 1;
            break;
        case 0xe1: // POP H
            state->l = state->memory[state->sp];
            state->h = state->memory[state->sp + 1];
            state->sp += 2;
            state->pc += 1;
            break;
        // --- Decrementos de 8 bits (DCR) ---
        case 0x05: state->b--; state->cc.z = (state->b == 0); state->cc.s = ((state->b & 0x80) != 0); state->pc += 1; break; // DCR B
        case 0x0d: state->c--; state->cc.z = (state->c == 0); state->cc.s = ((state->c & 0x80) != 0); state->pc += 1; break; // DCR C
        case 0x15: state->d--; state->cc.z = (state->d == 0); state->cc.s = ((state->d & 0x80) != 0); state->pc += 1; break; // DCR D
        case 0x1d: state->e--; state->cc.z = (state->e == 0); state->cc.s = ((state->e & 0x80) != 0); state->pc += 1; break; // DCR E
        case 0x25: state->h--; state->cc.z = (state->h == 0); state->cc.s = ((state->h & 0x80) != 0); state->pc += 1; break; // DCR H
        case 0x2d: state->l--; state->cc.z = (state->l == 0); state->cc.s = ((state->l & 0x80) != 0); state->pc += 1; break; // DCR L
        case 0x3d: state->a--; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->pc += 1; break; // DCR A

        // --- Incrementos de 8 bits (INR) ---
        case 0x04: state->b++; state->cc.z = (state->b == 0); state->cc.s = ((state->b & 0x80) != 0); state->pc += 1; break; // INR B
        case 0x0c: state->c++; state->cc.z = (state->c == 0); state->cc.s = ((state->c & 0x80) != 0); state->pc += 1; break; // INR C
        case 0x14: state->d++; state->cc.z = (state->d == 0); state->cc.s = ((state->d & 0x80) != 0); state->pc += 1; break; // INR D
        case 0x1c: state->e++; state->cc.z = (state->e == 0); state->cc.s = ((state->e & 0x80) != 0); state->pc += 1; break; // INR E
        case 0x24: state->h++; state->cc.z = (state->h == 0); state->cc.s = ((state->h & 0x80) != 0); state->pc += 1; break; // INR H
        case 0x2c: state->l++; state->cc.z = (state->l == 0); state->cc.s = ((state->l & 0x80) != 0); state->pc += 1; break; // INR L
        case 0x3c: state->a++; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->pc += 1; break; // INR A

        case 0x78: state->a = state->b; state->pc += 1; break; // MOV A, B
        case 0x79: state->a = state->c; state->pc += 1; break; // MOV A, C
        case 0x7a: state->a = state->d; state->pc += 1; break; // MOV A, D
        case 0x7b: state->a = state->e; state->pc += 1; break; // MOV A, E
        case 0x7c: state->a = state->h; state->pc += 1; break; // MOV A, H
        case 0x7d: state->a = state->l; state->pc += 1; break; // MOV A, L
        case 0x7f: state->a = state->a; state->pc += 1; break; // MOV A, A

        // --- Comparações (CPI / CMP) ---
        case 0xfe: { // CPI byte
            uint8_t byte = code[1];
            uint16_t res = state->a - byte;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (state->a < byte);
            state->pc += 2;
            break;
        }
        case 0xbb: { // CMP E
            uint16_t res = state->a - state->e;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (state->a < state->e);
            state->pc += 1;
            break;
        }

        // --- Operações Lógicas Imediatas (ANI / ORA / XRA) ---
        case 0xe6: { // ANI byte (AND imediato)
            state->a &= code[1];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0; // AND zera o Carry
            state->pc += 2;
            break;
        }
        case 0xf6: { // ORI byte (OR imediato)
            state->a |= code[1];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 2;
            break;
        }
        case 0xaf: { // XRA A (XOR A, A - forma clássica de zerar o registrador A)
            state->a = 0;
            state->cc.z = 1;
            state->cc.s = 0;
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }

        case 0x68: state->l = state->b; state->pc += 1; break; // MOV L, B
        case 0x69: state->l = state->c; state->pc += 1; break; // MOV L, C
        case 0x6a: state->l = state->d; state->pc += 1; break; // MOV L, E
        case 0x6b: state->l = state->e; state->pc += 1; break; // MOV L, D
        case 0x6c: state->l = state->h; state->pc += 1; break; // MOV L, H
        case 0x6d: state->l = state->l; state->pc += 1; break; // MOV L, L
        case 0x6e: state->l = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV L, M
        case 0x6f: state->l = state->a; state->pc += 1; break; // MOV L, A
        
        // --- Adição de 16 bits ao par HL (DAD) ---
        case 0x09: { // DAD B
            uint32_t hl = (state->h << 8) | state->l;
            uint32_t bc = (state->b << 8) | state->c;
            uint32_t res = hl + bc;
            state->h = (res >> 8) & 0xff;
            state->l = res & 0xff;
            state->cc.cy = (res > 0xffff);
            state->pc += 1;
            break;
        }
        case 0x19: { // DAD D
            uint32_t hl = (state->h << 8) | state->l;
            uint32_t de = (state->d << 8) | state->e;
            uint32_t res = hl + de;
            state->h = (res >> 8) & 0xff;
            state->l = res & 0xff;
            state->cc.cy = (res > 0xffff);
            state->pc += 1;
            break;
        }
        case 0x29: { // DAD H
            uint32_t hl = (state->h << 8) | state->l;
            uint32_t res = hl + hl;
            state->h = (res >> 8) & 0xff;
            state->l = res & 0xff;
            state->cc.cy = (res > 0xffff);
            state->pc += 1;
            break;
        }
        case 0x39: { // DAD SP
            uint32_t hl = (state->h << 8) | state->l;
            uint32_t res = hl + state->sp;
            state->h = (res >> 8) & 0xff;
            state->l = res & 0xff;
            state->cc.cy = (res > 0xffff);
            state->pc += 1;
            break;
        }

        case 0xeb: { // XCHG (troca HL com DE)
          uint8_t temp_h = state->h;
          uint8_t temp_l = state->l;
          
          state->h = state->d;
          state->l = state->e;
          
          state->d = temp_h;
          state->e = temp_l;
          
          state->pc += 1;
          break;
        }
        // --- Entrada / Saída (IN / OUT) ---
        // --- Saída de Dados (OUT) ---
        case 0xd3: { // OUT port
            uint8_t port = code[1];
            if (port == 2) {
                state->shift_offset = state->a & 0x07;
            } else if (port == 4) {
                state->shift_register = (state->a << 8) | (state->shift_register >> 8);
            }
            state->pc += 2;
            break;
        }

        // --- Rotações e Flags (RLC / RRC / RAL / RAR / CMA / STC / CMC) ---
        case 0x07: { // RLC (Rotate Left)
            uint8_t x = state->a;
            state->a = ((x & 0x80) >> 7) | (x << 1);
            state->cc.cy = (0x80 == (x & 0x80));
            state->pc += 1;
            break;
        }
        case 0x0f: { // RRC (Rotate Right)
            uint8_t x = state->a;
            state->a = ((x & 1) << 7) | (x >> 1);
            state->cc.cy = (1 == (x & 1));
            state->pc += 1;
            break;
        }
        case 0x17: { // RAL (Rotate Left through Carry)
            uint8_t x = state->a;
            state->a = state->cc.cy | (x << 1);
            state->cc.cy = (0x80 == (x & 0x80));
            state->pc += 1;
            break;
        }
        case 0x1f: { // RAR (Rotate Right through Carry)
            uint8_t x = state->a;
            state->a = (state->cc.cy << 7) | (x >> 1);
            state->cc.cy = (1 == (x & 1));
            state->pc += 1;
            break;
        }
        case 0x2f: state->a = ~state->a; state->pc += 1; break; // CMA (Complement A)
        case 0x37: state->cc.cy = 1; state->pc += 1; break;     // STC (Set Carry)
        case 0x3f: state->cc.cy = !state->cc.cy; state->pc += 1; break; // CMC (Complement Carry)

        // --- Operações Lógicas com Memória e Registradores (ANA / XRA / ORA) ---
        case 0xa0: state->a &= state->b; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // ANA B
        case 0xa7: state->a &= state->a; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // ANA A
        case 0xa8: state->a ^= state->b; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // XRA B
        case 0xb0: state->a |= state->b; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // ORA B
        case 0xb6: { // ORA M
            uint16_t offset = (state->h << 8) | state->l;
            state->a |= state->memory[offset];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }

        // --- Adição e Subtração de 8 bits (ADD / SUB / ADI / SUI) ---
        case 0x80: { uint16_t res = state->a + state->b; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADD B
        case 0x86: { uint16_t offset = (state->h << 8) | state->l; uint16_t res = state->a + state->memory[offset]; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADD M
        case 0xc6: { uint16_t res = state->a + code[1]; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 2; break; } // ADI byte
        case 0xd6: { uint16_t res = state->a - code[1]; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < code[1]); state->a = res & 0xff; state->pc += 2; break; } // SUI byte

        // --- Interrupções (EI / DI / RST) ---
        case 0xfb: state->int_enable = 1; state->pc += 1; break; // EI (Enable Interrupts)
        case 0xf3: state->int_enable = 0; state->pc += 1; break; // DI (Disable Interrupts)
        case 0xc7: case 0xcf: case 0xd7: case 0xdf:
        case 0xe7: case 0xef: case 0xf7: case 0xff: { // RST n
            uint16_t ret = state->pc + 1;
            state->memory[state->sp - 1] = (ret >> 8) & 0xff;
            state->memory[state->sp - 2] = ret & 0xff;
            state->sp -= 2;
            state->pc = *code & 0x38;
            break;
        }
        case 0x58: state->e = state->b; state->pc += 1; break; // MOV E, B
        case 0x59: state->e = state->c; state->pc += 1; break; // MOV E, C
        case 0x5a: state->e = state->d; state->pc += 1; break; // MOV E, D
        case 0x5b: state->e = state->e; state->pc += 1; break; // MOV E, E
        case 0x5c: state->e = state->h; state->pc += 1; break; // MOV E, H
        case 0x5d: state->e = state->l; state->pc += 1; break; // MOV E, L
        case 0x5e: state->e = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV E, M
        case 0x5f: state->e = state->a; state->pc += 1; break; // MOV E, A
        case 0x50: state->d = state->b; state->pc += 1; break; // MOV D, B
        case 0x51: state->d = state->c; state->pc += 1; break; // MOV D, C
        case 0x52: state->d = state->d; state->pc += 1; break; // MOV D, D
        case 0x53: state->d = state->e; state->pc += 1; break; // MOV D, E
        case 0x54: state->d = state->h; state->pc += 1; break; // MOV D, H
        case 0x55: state->d = state->l; state->pc += 1; break; // MOV D, L
        case 0x56: state->d = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV D, M
        case 0x57: state->d = state->a; state->pc += 1; break; // MOV D, A
        case 0x60: state->h = state->b; state->pc += 1; break; // MOV H, B
        case 0x61: state->h = state->c; state->pc += 1; break; // MOV H, C
        case 0x62: state->h = state->d; state->pc += 1; break; // MOV H, D
        case 0x63: state->h = state->e; state->pc += 1; break; // MOV H, E
        case 0x64: state->h = state->h; state->pc += 1; break; // MOV H, H
        case 0x65: state->h = state->l; state->pc += 1; break; // MOV H, L
        case 0x66: state->h = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV H, M
        case 0x67: state->h = state->a; state->pc += 1; break; // MOV H, A
        case 0xf5: { // PUSH PSW
            uint8_t psw = (state->cc.s << 7) |
                          (state->cc.z << 6) |
                          (state->cc.ac << 4) |
                          (state->cc.p << 2) |
                          (1 << 1) | // Bit 1 é sempre 1 no 8080
                          (state->cc.cy);
            state->memory[state->sp - 1] = state->a;
            state->memory[state->sp - 2] = psw;
            state->sp -= 2;
            state->pc += 1;
            break;
        }
        case 0xf1: { // POP PSW
            uint8_t psw = state->memory[state->sp];
            state->a = state->memory[state->sp + 1];
            state->cc.s  = (psw >> 7) & 1;
            state->cc.z  = (psw >> 6) & 1;
            state->cc.ac = (psw >> 4) & 1;
            state->cc.p  = (psw >> 2) & 1;
            state->cc.cy = psw & 1;
            state->sp += 2;
            state->pc += 1;
            break;
        }

        case 0x35: {
            uint16_t offset = (state->h << 8) | state->l;
            state->memory[offset]--;
            uint8_t res = state->memory[offset];
            
            // Atualiza as flags da CPU
            state->cc.z = (res == 0);
            state->cc.s = ((res & 0x80) != 0);
            
            state->pc += 1;
            break;
        }
        // --- Retorno Condicional (RZ) ---
        case 0xc8: { // RZ (Return if Zero)
            if (state->cc.z == 1) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else {
                state->pc += 1;
            }
            break;
        }
        // --- Retorno Condicional (RC) ---
        case 0xd8: { // RC (Return if Carry)
            if (state->cc.cy == 1) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else {
                state->pc += 1;
            }
            break;
        }
        // --- Cópia entre Registradores (MOV C, A) ---
        case 0x4f: state->c = state->a; state->pc += 1; break; // MOV C, A

        // --- Movimentação / Cópia de Dados Restante ---
        case 0x47: state->b = state->a; state->pc += 1; break; // MOV B, A
        case 0x70: state->memory[(state->h << 8) | state->l] = state->b; state->pc += 1; break; // MOV M, B
        case 0x71: state->memory[(state->h << 8) | state->l] = state->c; state->pc += 1; break; // MOV M, C
        case 0x72: state->memory[(state->h << 8) | state->l] = state->d; state->pc += 1; break; // MOV M, D
        case 0x73: state->memory[(state->h << 8) | state->l] = state->e; state->pc += 1; break; // MOV M, E

        // --- Incremento e Decremento de Memória (INR M) ---
        case 0x34: { // INR M
            uint16_t offset = (state->h << 8) | state->l;
            state->memory[offset]++;
            uint8_t res = state->memory[offset];
            state->cc.z = (res == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->pc += 1;
            break;
        }

        // --- Retornos Condicionais Restantes (RNZ / RNC / RM / RP) ---
        case 0xc0: { // RNZ (Return if Not Zero)
            if (state->cc.z == 0) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else {
                state->pc += 1;
            }
            break;
        }
        case 0xd0: { // RNC (Return if No Carry)
            if (state->cc.cy == 0) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else {
                state->pc += 1;
            }
            break;
        }

        // --- Saltos e Chamadas Condicionais Restantes (CNC / CZ / CNZ) ---
        case 0xd4: { // CNC (Call if No Carry)
            if (state->cc.cy == 0) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }
        case 0xc4: { // CNZ (Call if Not Zero)
            if (state->cc.z == 0) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }

        // --- Subtrações e Operações Lógicas Restantes ---
        case 0x97: { // SUB A
            state->a = 0;
            state->cc.z = 1;
            state->cc.s = 0;
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }
        case 0xa1: state->a &= state->c; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // ANA C
        case 0xb1: state->a |= state->c; state->cc.z = (state->a == 0); state->cc.s = ((state->a & 0x80) != 0); state->cc.cy = 0; state->pc += 1; break; // ORA C
        // --- Cópia de e para a Memória (MOV B, M e variantes) ---
        case 0x46: state->b = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOVO **`0x46`** é o **`MOV B, M`** (copia o byte de memória apontado pelo par de registradores `HL` para o registrador `B`).
        // --- Carga Direta em HL (LHLD adr) ---
        case 0x2a: {
            uint16_t offset = (code[2] << 8) | code[1];
            state->l = state->memory[offset];
            state->h = state->memory[offset + 1];
            state->pc += 3;
            break;
        }
        // --- Decrementos de 16 bits (DCX) ---
        case 0x2b: { // DCX H
            uint16_t hl = (state->h << 8) | state->l;
            hl--;
            state->h = (hl >> 8) & 0xff;
            state->l = hl & 0xff;
            state->pc += 1;
            break;
        }
        case 0x0b: { // DCX B
            uint16_t bc = (state->b << 8) | state->c;
            bc--;
            state->b = (bc >> 8) & 0xff;
            state->c = bc & 0xff;
            state->pc += 1;
            break;
        }
        case 0x1b: { // DCX D
            uint16_t de = (state->d << 8) | state->e;
            de--;
            state->d = (de >> 8) & 0xff;
            state->e = de & 0xff;
            state->pc += 1;
            break;
        }
        case 0x3b: state->sp--; state->pc += 1; break; // DCX SP
        // --- Troca do Topo da Pilha com HL (XTHL) ---
        case 0xe3: {
            uint8_t temp_l = state->l;
            uint8_t temp_h = state->h;

            state->l = state->memory[state->sp];
            state->h = state->memory[state->sp + 1];

            state->memory[state->sp] = temp_l;
            state->memory[state->sp + 1] = temp_h;

            state->pc += 1;
            break;
        }
        // --- PCHL (0xE9) - Carrega HL no PC ---
        case 0xe9: state->pc = (state->h << 8) | state->l; break; // PCHL

        // --- Ajuste Decimal (DAA - 0x27) ---
        case 0x27: {
            uint8_t res = state->a;
            if ((res & 0x0f) > 9 || state->cc.ac) {
                res += 6;
            }
            if (res > 0x9f || state->cc.cy) {
                res += 0x60;
                state->cc.cy = 1;
            }
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->a = res;
            state->pc += 1;
            break;
        }

        // --- SPHL (0xF9) - Carrega HL no SP ---
        case 0xf9: state->sp = (state->h << 8) | state->l; state->pc += 1; break;

        // --- Adição/Subtração com Carry (ADC / SBB / ADI / SBI) ---
        case 0x88: { // ADC B
            uint16_t res = state->a + state->b + state->cc.cy;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        case 0x8e: { // ADC M
            uint16_t offset = (state->h << 8) | state->l;
            uint16_t res = state->a + state->memory[offset] + state->cc.cy;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        case 0xce: { // ACI byte
            uint16_t res = state->a + code[1] + state->cc.cy;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xff);
            state->a = res & 0xff;
            state->pc += 2;
            break;
        }
        case 0x9f: { // SBB A
            uint16_t res = state->a - state->a - state->cc.cy;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (res > 0xffff);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        case 0xde: { // SBI byte
            uint16_t res = state->a - code[1] - state->cc.cy;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (state->a < (code[1] + state->cc.cy));
            state->a = res & 0xff;
            state->pc += 2;
            break;
        }

        // --- Subtrações Diretas e com Memória (SUB / SUI) ---
        case 0x90: { // SUB B
            uint16_t res = state->a - state->b;
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (state->a < state->b);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }
        case 0x96: { // SUB M
            uint16_t offset = (state->h << 8) | state->l;
            uint16_t res = state->a - state->memory[offset];
            state->cc.z = ((res & 0xff) == 0);
            state->cc.s = ((res & 0x80) != 0);
            state->cc.cy = (state->a < state->memory[offset]);
            state->a = res & 0xff;
            state->pc += 1;
            break;
        }

        // --- Escrita Direta da Memória via HL (SHLD) ---
        case 0x22: { // SHLD adr
            uint16_t offset = (code[2] << 8) | code[1];
            state->memory[offset] = state->l;
            state->memory[offset + 1] = state->h;
            state->pc += 3;
            break;
        }

        // --- Outros Saltos Condicionais (JM / JP / JPE / JPO) ---
        case 0xfa: if (state->cc.s == 1) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JM
        case 0xf2: if (state->cc.s == 0) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JP
        case 0xea: if (state->cc.p == 1) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JPE
        case 0xe2: if (state->cc.p == 0) state->pc = (code[2] << 8) | code[1]; else state->pc += 3; break; // JPO

        // --- Chamadas Condicionais Restantes (CC / CPE / CPO / CM / CP) ---
        case 0xdc: { // CC (Call if Carry)
            if (state->cc.cy == 1) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }
        case 0xfc: { // CM (Call if Minus)
            if (state->cc.s == 1) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }
        case 0xf4: { // CP (Call if Positive)
            if (state->cc.s == 0) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }

        // --- Comparações Restantes (CMP B/C/D/H/L/M) ---
        case 0xb8: { uint16_t res = state->a - state->b; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->b); state->pc += 1; break; } // CMP B
        case 0xb9: { uint16_t res = state->a - state->c; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->c); state->pc += 1; break; } // CMP C
        case 0xba: { uint16_t res = state->a - state->d; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->d); state->pc += 1; break; } // CMP D
        case 0xbc: { uint16_t res = state->a - state->h; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->h); state->pc += 1; break; } // CMP H
        case 0xbd: { uint16_t res = state->a - state->l; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->l); state->pc += 1; break; } // CMP L
        case 0xbe: { uint16_t offset = (state->h << 8) | state->l; uint16_t res = state->a - state->memory[offset]; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (state->a < state->memory[offset]); state->pc += 1; break; } // CMP M

        // --- Operações Lógicas Restantes (XRI / CPI) ---
        case 0xee: { // XRI byte
            state->a ^= code[1];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 2;
            break;
        }
        case 0xa6: { // ANA M
            uint16_t offset = (state->h << 8) | state->l;
            state->a &= state->memory[offset];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }
        case 0xae: { // XRA M
            uint16_t offset = (state->h << 8) | state->l;
            state->a ^= state->memory[offset];
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }
        // --- Chamada Condicional se Zero (CZ) ---
        case 0xcc: { // CZ adr
            if (state->cc.z == 1) {
                uint16_t ret = state->pc + 3;
                state->memory[state->sp - 1] = (ret >> 8) & 0xff;
                state->memory[state->sp - 2] = ret & 0xff;
                state->sp -= 2;
                state->pc = (code[2] << 8) | code[1];
            } else {
                state->pc += 3;
            }
            break;
        }
        // --- MOV C, M (0x4E) e restantes leituras/escritas de memória ---
        case 0x4e: state->c = state->memory[(state->h << 8) | state->l]; state->pc += 1; break; // MOV C, M
        case 0x74: state->memory[(state->h << 8) | state->l] = state->h; state->pc += 1; break; // MOV M, H
        case 0x75: state->memory[(state->h << 8) | state->l] = state->l; state->pc += 1; break; // MOV M, L

        // --- Retornos Condicionais que faltavam (RPE / RPO / RM / RP) ---
        case 0xe8: { // RPE (Return if Parity Even)
            if (state->cc.p == 1) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else { state->pc += 1; }
            break;
        }
        case 0xe0: { // RPO (Return if Parity Odd)
            if (state->cc.p == 0) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else { state->pc += 1; }
            break;
        }
        case 0xf8: { // RM (Return if Minus)
            if (state->cc.s == 1) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else { state->pc += 1; }
            break;
        }
        // --- Retorno Condicional se Positivo (RP) ---
        case 0xf0: { // RP (Return if Positive)
            if (state->cc.s == 0) {
                state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
                state->sp += 2;
            } else {
                state->pc += 1;
            }
            break;
        }
        // --- Operações Lógicas OR com Registadores (ORA H) ---
        case 0xb4: { // ORA H
            state->a |= state->h;
            state->cc.z = (state->a == 0);
            state->cc.s = ((state->a & 0x80) != 0);
            state->cc.cy = 0;
            state->pc += 1;
            break;
        }
        // --- MOV r, M (Cópia de Memória para Registadores) ---

        // --- MOV r, r' (Cópia entre Registadores que faltavam) ---
        case 0x40: state->b = state->b; state->pc += 1; break; // MOV B, B
        case 0x41: state->b = state->c; state->pc += 1; break; // MOV B, C
        case 0x42: state->b = state->d; state->pc += 1; break; // MOV B, D
        case 0x43: state->b = state->e; state->pc += 1; break; // MOV B, E
        case 0x44: state->b = state->h; state->pc += 1; break; // MOV B, H
        case 0x45: state->b = state->l; state->pc += 1; break; // MOV B, L

        case 0x48: state->c = state->b; state->pc += 1; break; // MOV C, B
        case 0x49: state->c = state->c; state->pc += 1; break; // MOV C, C
        case 0x4a: state->c = state->d; state->pc += 1; break; // MOV C, D
        case 0x4b: state->c = state->e; state->pc += 1; break; // MOV C, E
        case 0x4c: state->c = state->h; state->pc += 1; break; // MOV C, H
        case 0x4d: state->c = state->l; state->pc += 1; break; // MOV C, L

        // --- Restantes Adições com Carry (ADC) ---
        case 0x89: { uint16_t res = state->a + state->c + state->cc.cy; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADC C
        case 0x8a: { uint16_t res = state->a + state->d + state->cc.cy; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADC D
        case 0x8b: { uint16_t res = state->a + state->e + state->cc.cy; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADC E
        case 0x8c: { uint16_t res = state->a + state->h + state->cc.cy; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADC H
        case 0x8d: { uint16_t res = state->a + state->l + state->cc.cy; state->cc.z = ((res & 0xff) == 0); state->cc.s = ((res & 0x80) != 0); state->cc.cy = (res > 0xff); state->a = res & 0xff; state->pc += 1; break; } // ADC L
        default:
            printf("Opcode de execução ainda não implementado: 0x%02x no PC 0x%04x\n", *code, state->pc);
            exit(1);
    }
}

void GenerateInterrupt(State8080* state, int interruptNum){
    state->memory[state->sp - 1] = (state->pc >> 8) & 0xFF;
    state->memory[state->sp - 2] = state->pc & 0xFF;

    state->sp -= 2;

    state->pc = 8 * interruptNum;
    state->int_enable = 0;
}

int main(int argc, char** argv) {
    State8080* state = Init8080();

    // Carrega a ROM do jogo
    ReadFileToMemory(state, "invaders.h", 0x0000);
    ReadFileToMemory(state, "invaders.g", 0x0800);
    ReadFileToMemory(state, "invaders.f", 0x1000);
    ReadFileToMemory(state, "invaders.e", 0x1800);

    // Inicializa a SDL2
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Erro ao inicializar SDL: %s\n", SDL_GetError());
        return 1;
    }

    // Cria a janela (escala de 2x para 448x512)
    SDL_Window* window = SDL_CreateWindow("Space Invaders | 8080 Emulator",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2,
                                          SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            SCREEN_WIDTH, SCREEN_HEIGHT);

    uint32_t pixels[SCREEN_WIDTH * SCREEN_HEIGHT];

    int running = 1;

    int next_interrupt = 1;

    state->port1 = 0x09;

    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_c:     state->port1 |= 0x01; break; // Inserir Moeda (C)
                    case SDLK_1:     state->port1 |= 0x04; break; // Start 1 Player (1)
                    case SDLK_SPACE: state->port1 |= 0x10; break; // Tiro (Espaço)
                    case SDLK_LEFT:
                    case SDLK_a:     state->port1 |= 0x20; break; // Esquerda
                    case SDLK_RIGHT:
                    case SDLK_d:     state->port1 |= 0x40; break; // Direita
                }
            }

            if (event.type == SDL_KEYUP) {
                switch (event.key.keysym.sym) {
                    case SDLK_c:     state->port1 &= ~0x01; break;
                    case SDLK_1:     state->port1 &= ~0x04; break;
                    case SDLK_SPACE: state->port1 &= ~0x10; break;
                    case SDLK_LEFT:
                    case SDLK_a:     state->port1 &= ~0x20; break;
                    case SDLK_RIGHT:
                    case SDLK_d:     state->port1 &= ~0x40; break;
                }
            }
        
        }
        // --- Metade Superior da Tela (~16.666 ciclos) ---
        for (int i = 0; i < 2000; i++) {
            Emulate8080p(state);
        }
        if (state->int_enable) {
            GenerateInterrupt(state, 1); // RST 1 (0x0008)
        }

        // --- Metade Inferior da Tela (~16.666 ciclos) ---
        for (int i = 0; i < 2000; i++) {
            Emulate8080p(state);
        }
        if (state->int_enable) {
            GenerateInterrupt(state, 2); // RST 2 (0x0010)
        }

        RenderScreen(state, renderer, texture, pixels);
        SDL_Delay(16); // Manter ~60 FPS
    }

    // Limpeza
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    free(state->memory);
    free(state);
    return 0;
}