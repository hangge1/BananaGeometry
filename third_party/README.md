# 依赖策略

不修改第三方源码，也不将其提交到仓库。CMake FetchContent 自动获取以下固定发行包并校验哈希，缓存位于构建目录。首次配置需要网络，缓存完整后可以离线构建。只编译实际需要的文件。

| 依赖 | 版本 | 来源 | 发行包 SHA-256 |
| --- | --- | --- | --- |
| Dear ImGui | 1.91.9b | https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.9b | `8E1BBC76C71D74FEF2FB85DB7E7CA8EBA13D6A86623C54992B60162DB554FFDB` |
| Lua | 5.4.8 | https://www.lua.org/ftp/lua-5.4.8.tar.gz | `4F18DDAE154E793E46EEAB727C59EF1C0C0C2B744E7B94219710D76F530629AE` |
| Catch2（仅单元测试） | 3.16.1 | https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.16.1 | `D16BD1B8B2364918BD472CB216394ACB93BDE6784890DF0D91BCCFB3F28D777D` |

安装时从构建缓存复制 ImGui 和 Lua 的许可证到发布目录。升级时更新版本与哈希并重新运行自动化测试。

Catch2 仅在 `BUILD_TESTING=ON` 时获取和构建，不链接应用，也不安装到发布目录。

可使用标准 CMake 变量 `FETCHCONTENT_BASE_DIR` 指定缓存位置，或用 `FETCHCONTENT_SOURCE_DIR_IMGUI_SOURCE`、`FETCHCONTENT_SOURCE_DIR_LUA_SOURCE` 指向已解压的同版本源码，便于离线环境复用。无需单独安装 Conan、vcpkg 或 Lua 解释器。
