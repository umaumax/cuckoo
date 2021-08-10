#include <cstdio>
#include <iostream>
#include <string>

#include "cuckoo.hpp"

// NOTE: dummy
int add(int a, int b) { return 0; }

int add_hook(int a, int b) { return 100 + a + b; }

namespace {
void __attribute__((constructor)) init(void) {
  cuckoo::ElfData elf_data;
  bool ret = elf_data.OpenSelf();
  if (!ret) {
    printf("failed OpenSelf!\n");
    return;
  }
  ret = elf_data.ParseSymbol();
  if (!ret) {
    printf("failed ParseSymbol!\n");
    return;
  }
  printf("add(looked at hook lib)=%p\n", add);
  // elf_data.DumpSymbols();
  cuckoo::ElfSymbol* symbol = elf_data.GetFunctionSymbol("_Z3addii");
  if (symbol != nullptr) {
    printf("cuckoo::monkey_patch_wit_symbol call\n");
    printf("add(found by symbol info at hook lib)=%p\n", (void*)symbol->Addr());
    ret = cuckoo::monkey_patch_with_symbol(*symbol, (void*)add_hook);
  } else {
    printf("cuckoo::monkey_patch call\n");
    ret = cuckoo::monkey_patch((void*)add, (void*)add_hook);
  }
  if (!ret) {
    printf("failed monkey patching %s!\n", "add");
    return;
  }
}
}
