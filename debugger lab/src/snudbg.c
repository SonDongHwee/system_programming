#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <unistd.h>
#include <sys/personality.h>

#include "snudbg.h"
#include "procmaps.h"

int num_bps = 0;
breakpoint_t bps[MAX_BPS];
void dbg_wait();

/* HINT: No need to change this function */
void die(char* message) {
    WARN("Failed with message: '%s'\n", message);
    exit(-1);
}

/* HINT: No need to change this function */
void handle_regs(struct user_regs_struct *regs) {
    fprintf(stdout, "\t");
    PRINT_REG(rax);
    PRINT_REG(rbx);
    PRINT_REG(rcx);
    PRINT_REG(rdx);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(rbp);
    PRINT_REG(rsp);
    PRINT_REG(rsi);
    PRINT_REG(rdi);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(r8);
    PRINT_REG(r9);
    PRINT_REG(r10);
    PRINT_REG(r11);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(r12);
    PRINT_REG(r13);
    PRINT_REG(r14);
    PRINT_REG(r15);
    fprintf(stdout, "\n");

    fprintf(stdout, "\t");
    PRINT_REG(rip);
    PRINT_REG(eflags);
    fprintf(stdout, "\n");
}


/* HINT: No need to change this function */
void no_aslr(void) {
    unsigned long pv = PER_LINUX | ADDR_NO_RANDOMIZE;

    if (personality(pv) < 0) {
        if (personality(pv) < 0) {
            die("Failed to disable ASLR");
        }
    }
    return;
}

/* HINT: No need to change this function */
void tracee(char* cmd[]) {
    LOG("Tracee with pid=%d\n", getpid());

    no_aslr();
    
    if(ptrace(PTRACE_TRACEME, NULL, NULL, NULL)<0){
        die("Error traceing myself");
    }

    LOG("Loading the executable [%s]\n", cmd[0]);
    execvp(cmd[0], cmd);
}

/* INSTRUCTION: YOU SHOULD NOT CHANGE THIS FUNCTION */    
void dump_addr_in_hex(const ADDR_T addr, const void* data, size_t size) {
    uint i;
    for (i=0; i<size/16; i++) {
        printf("\t %llx ", addr+(i*16));
        for (uint j=0; j<16; j++) {
            printf("%02x ", ((unsigned char*)data)[i*16+j]);
        }
        printf("\n");
    }

    if (size%16 != 0) {
        // the rest
        printf("\t %llx ", addr+(i*16));
        for (uint j=0; j<size%16; j++) {
            printf("%02x ", ((unsigned char*)data)[i*16+j]);
        }
        printf("\n");
    }
}

/* HINT: No need to change this function */
void handle_help(void) {
    LOG("Available commands: \n");
    LOG("\t regs | get [REG] | set [REG] [value]\n");
    LOG("\t read [addr] [size] | write [addr] [value] [size]\n");
    LOG("\t step | continue | break [addr]\n");
    LOG("\t help\n");
    return;
}

void set_debug_state(int pid, enum debugging_state state) {
    if(state == SINGLE_STEP) {
        if(ptrace(PTRACE_SINGLESTEP, pid, NULL, NULL)<0) {
            die("Error tracing syscalls");
        }
    } else if (state == NON_STOP) {
        if(ptrace(PTRACE_CONT,pid,NULL,NULL)<0){
            die("Error tracing syscalls");
        }
    }
    return;
}


/* 
   Read the memory from @pid at the address @addr with the length @len.
   The data read from @pid will be written to @buf.
*/
void handle_read(int pid, ADDR_T addr, unsigned char *buf, size_t len) {
    // TODO: Use the function dump_addr_in_hex() to print the memory data
    size_t i = 0;
    for(i=0;i<len;i++){
        buf[i] = (unsigned char)ptrace(PTRACE_PEEKTEXT,pid,addr+i,NULL);
    }
    dump_addr_in_hex(addr,(const void *)(buf),len);
    return;
}

/* 
   Write the memory to @pid at the address @addr with the length @len.
   The data to be written is placed in @buf.
*/
void handle_write(int pid, ADDR_T addr, unsigned char *buf, size_t len) {
    // TODO
    long orig = ptrace(PTRACE_PEEKTEXT,pid,addr,NULL);
    long new;
    switch(len){
        case 1:
            new = strtol((char *)buf,NULL,16)|(orig&0xffffffffffffff00);
            break;
        case 2:
            new = strtol((char *)buf,NULL,16)|(orig&0xffffffffffff0000);
            break;
        case 3:
            new = strtol((char *)buf,NULL,16)|(orig&0xffffffffff000000);
            break;
        case 4:
            new = strtol((char *)buf,NULL,16)|(orig&0xffffffff00000000);
            break;
        case 5:
            new = strtol((char *)buf,NULL,16)|(orig&0xffffff0000000000);
            break;
        case 6:
            new = strtol((char *)buf,NULL,16)|(orig&0xffff000000000000);
            break;
        case 7:
            new = strtol((char *)buf,NULL,16)|(orig&0xff00000000000000);
            break;
        case 8:
            new = strtol((char *)buf,NULL,16)|(orig&0x0000000000000000);
            break;
        default: break;
    }
    ptrace(PTRACE_POKETEXT,pid,addr,new);
    return;
}

int get_rip(pid_t pid)
{
    struct user_regs_struct* regs;
    ptrace(PTRACE_GETREGS, pid, 0, &regs);
    return regs->rip;
}
void dbg_wait()
{
    int status;
    wait(&status);
}

/* 
   Install the software breakpoint at @addr to pid @pid.
*/
void handle_break(int pid, ADDR_T addr) {
    // TODO
    breakpoint_t *bp = (breakpoint_t*)malloc(sizeof(breakpoint_t));
    bp->addr = addr;
    bp->orig_value = ptrace(PTRACE_PEEKTEXT, pid, (void*)bp->addr, 0);
    long ins = ptrace(PTRACE_PEEKTEXT, pid, (void*)bp->addr, 0);
    //LOG("%x",bp->orig_value);
    ptrace(PTRACE_POKETEXT, pid, (void*)bp->addr,(ins&0xffffffffffffff00)|0xcc);
    bps[num_bps] = *bp;
    num_bps++;
}

#define CMPGET_REG(REG_TO_CMP)                   \
    if (strcmp(reg_name, #REG_TO_CMP)==0) {      \
        printf("\t");                            \
        PRINT_REG(REG_TO_CMP);                   \
        printf("\n");                            \
    }

/* HINT: No need to change this function */
void handle_get(char *reg_name, struct user_regs_struct *regs) {
    CMPGET_REG(rax); CMPGET_REG(rbx); CMPGET_REG(rcx); CMPGET_REG(rdx);
    CMPGET_REG(rbp); CMPGET_REG(rsp); CMPGET_REG(rsi); CMPGET_REG(rdi);
    CMPGET_REG(r8);  CMPGET_REG(r9);  CMPGET_REG(r10); CMPGET_REG(r11);
    CMPGET_REG(r12); CMPGET_REG(r13); CMPGET_REG(r14); CMPGET_REG(r15);
    CMPGET_REG(rip); CMPGET_REG(eflags);
    return;
}


/*
  Set the register @reg_name with the value @value.
  @regs is assumed to be holding the current register values of @pid.
*/
void handle_set(char *reg_name, unsigned long value,
                struct user_regs_struct *regs, int pid) {
    // TODO
    // LOG("%s %lu \n",reg_name,value);
    if(!strcmp(reg_name,"rax")) regs->rax = value;
    if(!strcmp(reg_name,"rbx")) regs->rbx = value;
    if(!strcmp(reg_name,"rcx")) regs->rcx = value;
    if(!strcmp(reg_name,"rdx")) regs->rdx = value;
    if(!strcmp(reg_name,"rbp")) regs->rbp = value;
    if(!strcmp(reg_name,"rsp")) regs->rsp = value;
    if(!strcmp(reg_name,"rsi")) regs->rsi = value;
    if(!strcmp(reg_name,"rdi")) regs->rdi = value;
    if(!strcmp(reg_name,"r8")) regs->r8 = value;
    if(!strcmp(reg_name,"r9")) regs->r9 = value;
    if(!strcmp(reg_name,"r10")) regs->r10 = value;
    if(!strcmp(reg_name,"r11")) regs->r11 = value;
    if(!strcmp(reg_name,"r12")) regs->r12 = value;
    if(!strcmp(reg_name,"r13")) regs->r13 = value;
    if(!strcmp(reg_name,"r14")) regs->r14 = value;
    if(!strcmp(reg_name,"r15")) regs->r15 = value;
    if(!strcmp(reg_name,"eflags")) regs->eflags = value;
    if(!strcmp(reg_name,"rip")) regs->rip = value;

    if(ptrace(PTRACE_SETREGS,pid,0,regs)<0)
        die("Error setting Regs");

    return;
}

void prompt_user(int child_pid, struct user_regs_struct *regs,
                 ADDR_T baseaddr) {
    TODO_UNUSED(child_pid);
    TODO_UNUSED(baseaddr);

    const char* prompt_symbol = ">>> ";

    for(;;) {
        fprintf(stdout, "%s", prompt_symbol);
        char action[1024];
        scanf("%1024s", action);
        if(strcmp("regs", action)==0) {
            handle_regs(regs);
            continue;
        }

        if(strcmp("help", action)==0 || strcmp("h", action)==0) {
            handle_help();
            continue;
        }

        if(strcmp("get", action)==0) {
            // TODO
            char reg[1024];
            scanf("%1024s", reg);
            LOG("HANDLE CMD: get [%s]\n",reg);
            handle_get(reg,regs);
            continue;
        }

        if(strcmp("set", action)==0) {
            // TODO
            char reg[1024], val_s[1024];
            scanf("%1024s",reg);
            scanf("%1024s",val_s);
            long unsigned val = strtol(val_s,NULL,16);
            LOG("HANDLE CMD: set [%s] as [0x%lx]\n",reg,val);
            handle_set(reg,(unsigned long)val,regs,child_pid);
            continue;
        }

        if(strcmp("read", action)==0 || strcmp("r", action)==0) {
            // TODO
            char addr_offset_c[1024];
            scanf("%1024s",addr_offset_c);
            ADDR_T addr_offset = (ADDR_T)strtol(addr_offset_c, NULL, 16);
            // Translated VM : construct_procmaps(child_pid)[0]->addr_begin + addr_offset;
            ADDR_T virtual_addr = construct_procmaps(child_pid)[0]->addr_begin + addr_offset;

            // read size
            char len_s[1024];
            scanf("%1024s",len_s);
            size_t len = strtol(len_s,NULL,16);

            LOG("HANDLE CMD: read [%llx][%llx] [%ld]\n",addr_offset,virtual_addr,len);
            unsigned char buf[len];
            handle_read(child_pid,virtual_addr,buf,len);
            
            continue;
        }

        if(strcmp("write", action)==0 || strcmp("w", action)==0) {
            // TODO
            char addr_offset_c[1024];
            scanf("%1024s",addr_offset_c);
            ADDR_T addr_offset = (ADDR_T)strtol(addr_offset_c, NULL, 16);
            // Translated VM : construct_procmaps(child_pid)[0]->addr_begin + addr_offset;
            ADDR_T virtual_addr = construct_procmaps(child_pid)[0]->addr_begin + addr_offset;

            //read value
            unsigned char val_s[1024];
            scanf("%1024s",val_s);
            long long val = strtol((char *)val_s,NULL,16);

            //read size
            char len_s[1024];
            scanf("%1024s",len_s);
            size_t len = strtol(len_s,NULL,16);
            
            LOG("HANDLE CMD: write [%llx][%llx] [%llx][%ld]\n",addr_offset,virtual_addr,val,len);
            //unsigned char *buf = 0;
            handle_write(child_pid,virtual_addr,val_s,len);
            continue;
        }

        if(strcmp("break", action)==0 || strcmp("b", action)==0) {
            // TODO
            char addr_offset_c[1024];
            scanf("%1024s",addr_offset_c);
            ADDR_T addr_offset = (ADDR_T)strtol(addr_offset_c, NULL, 16);
            //printf("%llu\n",addr_offset);

            // Translated VM : construct_procmaps(child_pid)[0]->addr_begin + addr_offset;
            ADDR_T virtual_addr = construct_procmaps(child_pid)[0]->addr_begin + addr_offset;
            LOG("HANDLE CMD: break [%llx][%llx]\n",addr_offset,virtual_addr);
            handle_break(child_pid,virtual_addr);
            continue;
        }

        if(strcmp("step", action)==0 || strcmp("s", action)==0) {
            // TODO
            set_debug_state(child_pid,SINGLE_STEP);
            break;
        }

        if(strcmp("continue", action)==0 || strcmp("c", action)==0) {
            // TODO
            set_debug_state(child_pid,NON_STOP);
            LOG("HANDLE CMD: continue\n");
            break;
        }

        if(strcmp("quit", action)==0 || strcmp("q", action)==0) {
            LOG("HANDLE CMD: quit\n");
            exit(0);
        }

        WARN("Not available commands\n");
    }
}


/*
  Get the current registers of @pid, and store it to @regs.
*/
void get_registers(int pid, struct user_regs_struct *regs) {
    if(ptrace(PTRACE_GETREGS, pid, NULL, regs)<0) {
        die("Error getting registers");
    }
    return;
}


/*
  Set the registers of @pid with @regs.
*/
void set_registers(int pid, struct user_regs_struct *regs) {
    // TODO
  
    if(ptrace(PTRACE_SETREGS,pid,0,regs)<0) {
        die("Error setting registers");
    }
}


/*
  Get the base address of the main binary image, 
  loaded to the process @pid.
  This base address is the virtual address.
*/
ADDR_T get_image_baseaddr(int pid) {
    hr_procmaps** procmap = construct_procmaps(pid);
    ADDR_T baseaddr = 0;
    // TODO
    baseaddr = procmap[0]->addr_begin;
    return baseaddr;
}

/*
  Perform the job if the software breakpoint is fired.
  This includes to restore the original value at the breakpoint address.
*/
void handle_break_post(int pid, struct user_regs_struct *regs) {
    // TODO
    if(num_bps == 0) return;

    int i = 0; // Search for bp in bps
    for(i=0;i<num_bps;i++){
        if(bps[i].addr == (regs->rip)-1){
            break;
        }
    }

    if(i >= num_bps) return; // NOT Founded

    LOG("        FOUND MATCH BP: [%d] [%llx][%x]\n",i,bps[i].addr,bps[i].orig_value);

    long soft_bp = ptrace(PTRACE_PEEKTEXT, pid, (void*)bps[i].addr, 0);
    long ins = (soft_bp&0xffffffffffffff00)|bps[i].orig_value;

    
    regs->rip -= 1; // addr of bp
    if(ptrace(PTRACE_SETREGS,pid,0,regs)<0) {
        die("Error setting registers");
    }

    if(ptrace(PTRACE_POKEDATA, pid, (void*)bps[i].addr, ins)<0){
        die("POKE failed");
    }
    bps[i].addr = 0;
    bps[i].orig_value = 0;
    for(int j=i;j<num_bps;j++){
        bps[j] = bps[j+1];
    }
    num_bps--;
}


/* HINT: No need to change this function */
void tracer(int child_pid) {
    int child_status;

    LOG("Tracer with pid=%d\n", getpid());

    wait(&child_status);

    ADDR_T baseaddr = get_image_baseaddr(child_pid);

    int steps_count = 0;
    struct user_regs_struct tracee_regs;
    set_debug_state(child_pid, SINGLE_STEP);

    while(1) {
        wait(&child_status);
        steps_count += 1;

        if(WIFEXITED(child_status)) {
            LOG("Exited in %d steps with status=%d\n",
                steps_count, child_status);
            break;
        }
        get_registers(child_pid, &tracee_regs);

        LOG("[step %d] rip=%llx child_status=%d\n", steps_count,
            tracee_regs.rip, child_status);

        handle_break_post(child_pid, &tracee_regs);
        prompt_user(child_pid, &tracee_regs, baseaddr);
    }
}

/* HINT: No need to change this function */
int main(int argc, char* argv[]) {
    char* usage = "USAGE: ./snudbg <cmd>";

    if (argc < 2){
        die(usage);
    }

    int pid = fork();

    switch (pid) {
    case -1:
        die("Error forking");
        break;
    case 0:
        tracee(argv+1);
        break;
    default:
        tracer(pid);
        break;
    }
    return 0;
}
