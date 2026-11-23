# reactor 库打包说明（pm 包）

## 打包

```bash
./pack.sh        # 编译 debug/release 两套 libreactor.a，发布到 ~/.project_manager/repo/
./pack.sh -f     # 同名同版本已存在时强制覆盖
```

产物：

- `~/.project_manager/repo/reactor-0.1.1.pm`
- `~/.project_manager/repo/reactor-0.1.1.pm.sha256`

包内结构：

```text
reactor/
├── manifest.json        # link_libs=[reactor]，依赖 util/system/protocol/algorithm/basic/data_structure
├── include/*.h          # 安装到 extern_inc/reactor/
└── lib/{debug,release}/libreactor.a
```

## 使用

```bash
pm deps add reactor -p MyApp
```

```cpp
#include <reactor/reactor.h>
```

## 测试

```bash
pm deps install reactor
pm build reactor
```

会构建 `libreactord.a` 以及 `http_chunk_test`、`http_test`、`echo_server_test`、
`tcp_client_test`、`tcp_server_test`。这些测试是网络/服务端集成测试，需要按场景手工运行。

## 升级版本

同步修改 `.pmconfig/config.json`、`pack/manifest.json`、`pack/pack.sh` 中的版本号，然后执行 `./pack.sh`。
