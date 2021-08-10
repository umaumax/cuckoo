# cuckoo

monkey patching tool for c++
+
## how to use
``` bash
g++ -std=c++11 -O3 -shared -fPIC example_hook.cpp cuckoo.cpp -o libexample_hook.so

# シンボルを公開しない場合は、何らかの方法で見つける必要がある
g++ -std=c++11 example.cpp -o example
#シンボルを公開すれば簡単
g++ -std=c++11 example.cpp -o example -rdynamic

LD_PRELOAD=./libexample_hook.so ./example
```

## how to search symbol inside
### without ASLR
事前に下記で見つけたアドレスを利用するか、実行時にelfファイルを解析する
``` bash
readelf -s -W example | grep add
```

### with ASLR
WIP

## architecture
* [x] x86_64
* [?] x86
* [ ] aarch32
* [ ] aarch64

## TODO
* 置き換え前の関数を呼ぶ機構の追加
* テストの追加
