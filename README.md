this project makes excessive use of function blocks a C extension that only works if you link the [BlocksRuntime](https://github.com/mackyle/blocksruntime) at compile time . after installing BlocksRuntime into your include and lib folders , compile klang.c using these flags: 

```
clang -fblocks -lBlocksRuntime klang.c
```
