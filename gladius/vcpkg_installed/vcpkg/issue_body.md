Package: eigen3:x64-linux@3.4.0#5

**Host Environment**

- Host: x64-linux
- Compiler: GNU 13.3.0
-    vcpkg-tool version: 2025-06-20-ef7c0d541124bbdd334a03467e7edb6c3364d199
    vcpkg-scripts version: cd6256cd37 2025-07-10 (7 days ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz -> libeigen-eigen-3.4.0.tar.gz
warning: Problem : timeout. Will retry in 1 seconds. 3 retries left.
warning: Problem : timeout. Will retry in 2 seconds. 2 retries left.
warning: Problem : timeout. Will retry in 4 seconds. 1 retries left.
error: curl: (6) Could not resolve host: gitlab.com
note: If you are using a proxy, please ensure your proxy settings are correct.
Possible causes are:
1. You are actually using an HTTP proxy, but setting HTTPS_PROXY variable to `https//address:port`.
This is not correct, because `https://` prefix claims the proxy is an HTTPS proxy, while your proxy (v2ray, shadowsocksr, etc...) is an HTTP proxy.
Try setting `http://address:port` to both HTTP_PROXY and HTTPS_PROXY instead.
2. If you are using Windows, vcpkg will automatically use your Windows IE Proxy Settings set by your proxy software. See: https://github.com/microsoft/vcpkg-tool/pull/77
The value set by your proxy might be wrong, or have same `https://` prefix issue.
3. Your proxy's remote server is our of service.
If you believe this is not a temporary download server failure and vcpkg needs to be changed to download this file from a different location, please submit an issue to https://github.com/Microsoft/vcpkg/issues
CMake Error at scripts/cmake/vcpkg_download_distfile.cmake:136 (message):
  Download failed, halting portfile.
Call Stack (most recent call first):
  scripts/cmake/vcpkg_from_gitlab.cmake:113 (vcpkg_download_distfile)
  buildtrees/versioning_/versions/eigen3/d9b547a9e3dc5b847f5ecab763fdea9728107a16/portfile.cmake:6 (vcpkg_from_gitlab)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "dependencies": [
    "fmt",
    "openmesh",
    "glad",
    "glfw3",
    "eigen3",
    "tinyfiledialogs",
    "gtest",
    "platform-folders",
    "opencl",
    {
      "name": "openvdb",
      "features": [
        "nanovdb"
      ]
    },
    {
      "name": "imgui",
      "platform": "windows",
      "features": [
        "docking-experimental",
        "glfw-binding",
        "opengl2-binding",
        "win32-binding"
      ]
    },
    {
      "name": "imgui",
      "platform": "!windows",
      "features": [
        "docking-experimental",
        "glfw-binding",
        "opengl2-binding"
      ]
    },
    "imgui-node-editor",
    "cmakerc",
    "blosc",
    "zlib",
    "lodepng",
    "minizip",
    "pugixml",
    "lib3mf",
    "pkgconf",
    "minizip",
    "tracy",
    "clipper2",
    "nlohmann-json"
  ],
  "overrides": [
    {
      "name": "imgui",
      "version": "1.89.9"
    },
    {
      "name": "imgui-node-editor",
      "version": "0.9.2"
    }
  ],
  "name": "gladius",
  "builtin-baseline": "f2bf7d935af722c0cd8219c1f6e30d5ae3d666f2",
  "version-string": "1.2.13"
}

```
</details>
