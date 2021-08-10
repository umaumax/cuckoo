#include <cassert>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "cuckoo.hpp"

namespace cuckoo {
#ifdef __linux__
#define PAGE_SIZE (4096)

bool monkey_patch(void *original_func_ptr, const void *new_func_ptr) {
#ifdef __x86_64__
#define ASM_SIZE 12
  char op[ASM_SIZE];
  // movq
  op[0] = 0x48;
  op[1] = 0xb8;
  // op[2~9] 8B
  uintptr_t *address = (uintptr_t *)&op[2];
  *address           = (uintptr_t)new_func_ptr;
  // jmpq
  op[10] = 0xff;
  op[11] = 0xe0;
#elif defined(__i386__)
#define ASM_SIZE 5
  char op[ASM_SIZE];
  // jmp
  op[0] = 0xe9;
  // op[1~4] 4B
  uintptr_t *address = (uintptr_t *)&op[1];
  *address           = new_func_ptr - original_func_ptr - ASM_SIZE;
#elif defined(__aarch64__)
#error "not supported arch yet"
#elif defined(__arm__)
#error "not supported arch yet"
#else
#error "not supported arch"
#endif
  void *original_func_ptr_offset =
      (void *)((uintptr_t)original_func_ptr -
               (((uintptr_t)original_func_ptr) % PAGE_SIZE));
  size_t length = (((uintptr_t)original_func_ptr + (PAGE_SIZE - 1) + ASM_SIZE) /
                   PAGE_SIZE) *
                      PAGE_SIZE -
                  ((uintptr_t)original_func_ptr -
                   ((uintptr_t)original_func_ptr % PAGE_SIZE));
  if (mprotect(original_func_ptr_offset, length,
               PROT_READ | PROT_WRITE | PROT_EXEC) == -1) {
    std::cerr << "mprotect failed: " << std::strerror(errno) << std::endl;
    return false;
  }
  memcpy(original_func_ptr, op, ASM_SIZE);
  return true;
}
#endif

bool monkey_patch_with_symbol(const ElfSymbol &symbol,
                              const void *new_func_ptr) {
  void *original_func_ptr = (void *)symbol.Addr();
  const size_t size       = symbol.Size();
  if (size < ASM_SIZE) {
    fprintf(stderr, "%s function has %luB: requires %dB\n",
            symbol.Name().c_str(), size, ASM_SIZE);
    return false;
  }
  return monkey_patch(original_func_ptr, new_func_ptr);
}

bool ElfData::Close() {
  if (addr_ != nullptr) {
    int ret = munmap(addr_, size_);
    if (ret == -1) {
      fprintf(stderr, "mmap(): %s: %s\n", filepath_.c_str(), strerror(errno));
      return false;
    }
  }
}
bool ElfData::Open(const std::string &filepath) {
  int fd = open(filepath.c_str(), O_RDONLY);

  if (fd < 0) {
    fprintf(stderr, "open(): %s: %s\n", filepath.c_str(), strerror(errno));
    return false;
  }

  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    fprintf(stderr, "fstat(): %s: %s\n", filepath.c_str(), strerror(errno));
    return false;
  }
  int size = file_stat.st_size;

  const int offset = 0;
  void *addr       = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, offset);
  if (addr == MAP_FAILED) {
    fprintf(stderr, "mmap(): %s: %s\n", filepath.c_str(), strerror(errno));
    return false;
  }

  int ret = close(fd);
  if (ret == -1) {
    fprintf(stderr, "close(): %s: %s\n", filepath.c_str(), strerror(errno));
    return false;
  }

  filepath_ = filepath;
  addr_     = addr;
  size_     = size;
  return true;
}

bool ElfData::OpenSelf() {
  pid_t pid = getpid();
  // symbolic link
  std::string self_exe_filepath =
      std::string("/proc/") + std::to_string(pid) + std::string("/exe");
  return Open(self_exe_filepath);
}

void ElfData::ParseDynSymSectionHeaderToSymbols(const ElfW(Shdr) * shdr,
                                                const size_t strtab_offset) {
  std::vector<ElfSymbol> symbols;
  ParseSectionHeaderToSymbols((const ElfW(Ehdr) *)addr_, shdr, strtab_offset,
                              symbols);
  symbols_map_.emplace("dynsym", symbols);
}

void ElfData::ParseSymTabSectionHeaderToSymbols(const ElfW(Shdr) * shdr,
                                                const size_t strtab_offset) {
  std::vector<ElfSymbol> symbols;
  ParseSectionHeaderToSymbols((const ElfW(Ehdr) *)addr_, shdr, strtab_offset,
                              symbols);
  symbols_map_.emplace("symtab", symbols);
}

void ElfData::ParseSectionHeaderToSymbols(const ElfW(Ehdr) * ehdr,
                                          const ElfW(Shdr) * shdr,
                                          const size_t strtab_offset,
                                          std::vector<ElfSymbol> &symbols) {
  assert(ehdr != nullptr);
  assert(shdr != nullptr);
  assert(symbols.size() == 0);

  ElfW(Sym) *syments  = (ElfW(Sym) *)((uintptr_t)ehdr + shdr->sh_offset);
  size_t nsyms        = shdr->sh_size / sizeof(ElfW(Sym));
  const char *symstrs = (char *)((uintptr_t)ehdr + strtab_offset);

  for (int i = 0; i < nsyms; i++) {
    ElfW(Sym) &syment = syments[i];
    size_t nameidx    = syment.st_name;
    if (nameidx == 0 || syment.st_value == 0 || syment.st_info == STT_SECTION ||
        syment.st_info == STT_FILE) {
      continue;
    }
    std::string symname(symstrs + nameidx);

    uintptr_t addr = syment.st_value;
    size_t size    = syment.st_size;
    ElfSymbol symbol(symname, addr, size);
    symbols.emplace_back(symbol);
  }
}

bool ElfData::ParseSymbol() {
  ElfW(Ehdr) *ehdr        = (Elf64_Ehdr *)addr_;
  ElfW(Shdr) *shdr        = nullptr;
  ElfW(Shdr) *sym_shdr    = nullptr;
  ElfW(Shdr) *dynsym_shdr = nullptr;
  ElfW(Sym) *syments      = nullptr;
  ElfW(Sym) *symvictim    = nullptr;

  char *strtable           = nullptr;
  size_t symstrs_offset    = 0;
  size_t dynsymstrs_offset = 0;
  size_t stridx;
  size_t nsyms;

  stridx   = ehdr->e_shstrndx;
  shdr     = (ElfW(Shdr) *)((uintptr_t)ehdr + ehdr->e_shoff);
  strtable = (char *)((uintptr_t)ehdr + shdr[stridx].sh_offset);

  for (int i = 0; i < ehdr->e_shnum; i++) {
    std::string section_name;
    size_t nameidx = shdr[i].sh_name;

    if (nameidx) {
      section_name = std::string(strtable + nameidx);
    }
    // for debug
    // printf("section_name=%s\n", section_name.c_str());

    // for debug
    // ElfW(Shdr) &symbol = shdr[i];
    // printf(
    // "SECTION: \"%s\": "
    // "idx=%d, type=%" PRIx64 " , flags=%" PRIx64 ", info=%" PRIx32 "\n",
    // section_name.c_str(), i, (uint64_t)symbol.sh_type,
    // (uint64_t)symbol.sh_flags, (uint32_t)symbol.sh_info);

    switch (shdr[i].sh_type) {
      case SHT_SYMTAB:
        sym_shdr = &shdr[i];
        break;
      case SHT_DYNSYM:
        dynsym_shdr = &shdr[i];
        break;
      case SHT_STRTAB: {
        size_t offset = shdr[i].sh_offset;
        if (section_name == ".strtab") {
          symstrs_offset = offset;
        } else if (section_name == ".dynstr") {
          dynsymstrs_offset = offset;
        }
        break;
      }
      default:
        break;
    }
  }

  if (sym_shdr) {
    ParseSymTabSectionHeaderToSymbols(sym_shdr, symstrs_offset);
  }
  if (dynsym_shdr) {
    ParseDynSymSectionHeaderToSymbols(dynsym_shdr, dynsymstrs_offset);
  }
  if (sym_shdr == nullptr && dynsym_shdr == nullptr) {
    printf("Couldn't find symbol table!\n");
    printf("Couldn't find symbol string table!\n");
    return false;
  }
  return true;
}

void ElfData::DumpSymbols() {
  std::vector<ElfSymbol> symtab_symbols = GetSymTabSymbols();
  std::vector<ElfSymbol> dynsym_symbols = GetDynSymSymbols();

  for (auto &v : symtab_symbols) {
    printf("%s(addr=%p,size=%luB)\n", v.Name().c_str(), (void *)v.Addr(),
           v.Size());
  }
  for (auto &v : dynsym_symbols) {
    printf("%s(addr=%p,size=%luB)\n", v.Name().c_str(), (void *)v.Addr(),
           v.Size());
  }
}
}
