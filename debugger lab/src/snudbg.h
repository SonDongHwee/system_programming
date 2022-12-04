#ifndef _SNUDBG_H_
#define _SNUDBG_H_

enum debugging_state {
    SINGLE_STEP,
    NON_STOP
};

#define LOG(...) fprintf(stdout, "[*] " __VA_ARGS__)
#define WARN(...) fprintf(stdout, "[-] " __VA_ARGS__)

#define MAX_PATH 1024
#define MAX_LINE 1024

#define ADDR_T unsigned long long

#define PRINT_REG(REG_STR) printf("%s=0x%llx ", #REG_STR, regs->REG_STR)

#define MAX_BPS 4

struct breakpoint {
    ADDR_T addr;
    unsigned char orig_value;
};
typedef struct breakpoint breakpoint_t;

#define MAX_RW 1024
#define RW_UNIT 8

void set_registers(int pid, struct user_regs_struct *tracee_regs);
void get_registers(int pid, struct user_regs_struct *tracee_regs);

#define TODO_UNUSED(x) (void)(x)

#endif
