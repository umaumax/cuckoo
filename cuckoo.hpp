#ifndef CUCKOO_HPP_INCLUDED
#define CUCKOO_HPP_INCLUDED

#include <elf.h>
#include <link.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cuckoo {
class ElfSymbol;
bool monkey_patch(void *original_func_ptr, const void *new_func_ptr);
bool monkey_patch_with_symbol(const ElfSymbol &symbol,
                              const void *new_func_ptr);

class ElfSymbol {
 public:
  ElfSymbol() {}
  ElfSymbol(std::string name, uintptr_t addr, size_t size)
      : name_(name), addr_(addr), size_(size) {}

  std::string Name() const { return name_; }
  uintptr_t Addr() const { return addr_; }
  size_t Size() const { return size_; }

 private:
  std::string name_;
  uintptr_t addr_ = 0;
  size_t size_    = 0;
};

class ElfData {
 public:
  ElfData() {}
  ~ElfData() { Close(); }
  bool Close();
  bool Open(const std::string &filepath);
  bool OpenSelf();
  bool ParseSymbol();
  void *StartAddress() { return addr_; }
  void ParseDynSymSectionHeaderToSymbols(const ElfW(Shdr) * shdr,
                                         const size_t strtab_offset);
  void ParseSymTabSectionHeaderToSymbols(const ElfW(Shdr) * shdr,
                                         const size_t strtab_offset);
  std::vector<ElfSymbol> GetDynSymSymbols() { return symbols_map_["dynsym"]; }
  std::vector<ElfSymbol> GetSymTabSymbols() { return symbols_map_["symtab"]; }
  void DumpSymbols();
  ElfSymbol *GetFunctionSymbol(const std::string &symbol_name) {
    auto dynsym_symbols = GetDynSymSymbols();
    {
      auto result = std::find_if(
          dynsym_symbols.begin(), dynsym_symbols.end(),
          [&symbol_name](ElfSymbol &sym) { return sym.Name() == symbol_name; });
      if (result != dynsym_symbols.end()) {
        return &(*result);
      }
    }
    auto symtab_symbols = GetSymTabSymbols();
    {
      auto result = std::find_if(
          symtab_symbols.begin(), symtab_symbols.end(),
          [&symbol_name](ElfSymbol &sym) { return sym.Name() == symbol_name; });
      if (result != dynsym_symbols.end()) {
        return &(*result);
      }
    }
    return nullptr;
  }

  static void ParseSectionHeaderToSymbols(const ElfW(Ehdr) * ehdr,
                                          const ElfW(Shdr) * shdr,
                                          const size_t strtab_offset,
                                          std::vector<ElfSymbol> &symbols);

 private:
  std::unordered_map<std::string, std::vector<ElfSymbol>> symbols_map_;
  std::string filepath_;
  void *addr_  = nullptr;
  size_t size_ = 0;
};
}

#endif  // CUCKOO_HPP_INCLUDED
