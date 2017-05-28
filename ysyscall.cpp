#include "ysyscall.h"
#include "vm.h"
#include "cpu.h"
#include "elf.h"
#include <set>

int yfork(int &error,int parent_pid,std::map<int,VM*>* pid_map,VM *old_vm){
    int new_pid;
    for(int i=1;;++i){
        if(pid_map->count(i)==0){
            new_pid = i;
            break;
        }
    }
    VM* new_vm = new VM(*old_vm);
    new_vm->pid = new_pid;
    //set all the PRIVATE pages to RDONLY(copy-on-write)
    for(auto i = old_vm->vm_area.begin();i!=old_vm->vm_area.end();++i){
        if(i->flags == VM::VM_AREA_STRUCT::PRIVATE && i->prot == VM::VM_AREA_STRUCT::RW)
            i->prot = VM::VM_AREA_STRUCT::RONLY;
    }
    for(auto i = new_vm->vm_area.begin();i!=new_vm->vm_area.end();++i){
        if(i->flags == VM::VM_AREA_STRUCT::PRIVATE && i->prot == VM::VM_AREA_STRUCT::RW)
            i->prot = VM::VM_AREA_STRUCT::RONLY;
    }
    error =0 ;
    return new_pid;
}

int yexecve(int &error,ELF* elf,VM* vm){
    //destroy the user-space of the VM
    for(auto i = vm->pgd.begin();i!=vm->pgd.end();++i){
        WORD vm_addr = i->first;
        if(vm->pgd_valid[vm_addr]){
            vm->free_page_pte(vm_addr,error);
        }
    }
    //build vm space
    WORD next_addr = vm->allocate_section("text",0,elf->text,elf->text_len,
                                          VM::VM_AREA_STRUCT::RONLY,VM::VM_AREA_STRUCT::PRIVATE,error);
    next_addr = vm->allocate_section("data",next_addr,elf->data,elf->data_len,
                                     VM::VM_AREA_STRUCT::RW,VM::VM_AREA_STRUCT::PRIVATE,error);
    BYTE *bss = new BYTE[elf->bss_len];
    for(int i =0;i<elf->bss_len;++i){
        bss[i] = 0;
    }
    next_addr = vm->allocate_section("bss",next_addr,bss,elf->bss_len,
                                     VM::VM_AREA_STRUCT::RW,VM::VM_AREA_STRUCT::PRIVATE,error);

    vm->heap_low = vm->heap_high = next_addr;
    vm->stack_low = vm->stack_high = 0x80000000;
    vm->elf_text = elf->string_text;
}

int context_switch(int &error,VM* old_vm,VM* new_vm,CPU* cpu,int running_core){
    Core* core = cpu->core[running_core];
    if(old_vm!=NULL){
        old_vm->saved_core = core;
    }
    if(new_vm->saved_core!=NULL){
        cpu->core[running_core] = new_vm->saved_core;
    }
    else{
        cpu->core[running_core] = new Core(cpu);
        cpu->core[running_core]->REG[4]=cpu->core[running_core]->REG[5] = new_vm->stack_high;
    }
    cpu->core[running_core]->vm = new_vm;

}