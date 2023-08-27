# cuckoo

monkey patching tool for c++

## how to use
``` bash
g++ -std=c++11 -O3 -shared -fPIC example_hook.cpp cuckoo.cpp -o libexample_hook.so

# シンボルを公開しない場合は、何らかの方法で見つける必要がある
g++ -std=c++11 example.cpp -o example
# シンボルを公開すれば簡単
g++ -std=c++11 example.cpp -o example -rdynamic

LD_PRELOAD=./libexample_hook.so ./example
```

## how to search symbol inside
### without ASLR
use address by below command or parse elf file while runtime
``` bash
readelf -s -W example | grep add
```

### with ASLR
WIP

## architecture
### Linux
* [x] x86_64
* [?] x86
* [ ] aarch32
* [ ] aarch64

### Mac
You cannot use this because of `mprotect` error!

## TODO
* 置き換え前の関数を呼ぶ機構の追加
  * コンパイラの`-fpatchable-function-entry=N[,M]`によって、nopが連続している判定を追加して、元の関数を呼べるアドレスを記録する?
* テストの追加
* use `getpagesize()` instead of `4096`
