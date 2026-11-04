# 《坦克大战》(krystiankaluzny/Tanks) 移植到 Android 施工文档

> 目标:把 C++/SDL2 的桌面版 Battle City 复刻跑在 Android 上(手机 / Android TV)。
> 用法:本文件按「阶段」组织,每阶段都有 **命令 → 验收标准 → 卡住时的退路**。
> 换电脑后从「阶段 P」的恢复检查开始,按 `[ ]` 勾选继续即可。

---

## P. 进度看板(每次开工先看这里)

> 📁 **代码仓库**:https://github.com/zccguagua/TankTv (分支 `main`)

最近更新:**2026-11-04**(在这台机器上已实际执行并通过)

| 阶段 | 状态 | 产物 / 证据 |
|---|---|---|
| 0 环境 | ✅ 完成 | JDK 17.0.10;SDK `D:/Sdk`(platform 34/35/36);NDK `android-ndk-r26d`;CMake 3.22.1 |
| A 桌面基线 | ⏭️ **跳过** | 非必需,直接进 Android |
| B 模板 APK | ✅ **完成** | `app/app/build/outputs/apk/debug/app-debug.apk`(1.2MB)构建成功 |
| C 接入游戏源码 | ⬜ **下一步** | 游戏源码已就位 `game/`(34 个 .cpp) |
| D 资源解包 | ⬜ 未开始 | — |
| E 声音 | ⬜ 未开始 | — |
| F 输入 | ⬜ 未开始 | — |
| G 文字 | ⬜ 未开始 | — |
| H/I/J 收尾 | ⬜ 未开始 | — |

**下一步要做的第一件事**:把 `app/app/jni/src/CMakeLists.txt` 里的 `main.c` 换成游戏的 `game/src/**/*.cpp`,见第 5 节。

> ⚠️ **本机的三个环境坑(已经在下面 B.2 记录,换机前务必先看)**:
>
> 1. `github.com` 直连被封 → 拉源码改走 `codeload.github.com` 下载 zip(可用)
> 2. `D:\.gradle\gradle.properties` 里写死了 `127.0.0.1:7890` 代理,代理不开时 Gradle 全部失败 → 构建时需临时禁用
> 3. Gradle 官方发行包会被重定向到 github.com(被封)→ 用腾讯云镜像下载,并用 `D:/tools/gradle-8.1.1/bin/gradle` 直接构建(绕开 wrapper)

---

## 0. 先读这三段

**结论:可以移植,而且源码结构对移植友好。** 依据(已核实):

- 仓库:https://github.com/krystiankaluzny/Tanks ,**MIT 许可**,默认分支 `master`。
- 它**没有任何 Android 移植**:issue/PR 里搜 `android` 为 0 条;仓库只有 MinGW 的 `Makefile` 和 Windows 预编译 DLL(`resources/SDL/SDL2-2.32.4/…` x86/x64)。所以你是第一个。
- 依赖:SDL2 **2.32.4**、SDL2_image **2.8.8**、SDL2_mixer **2.8.1**、SDL2_ttf **2.24.0**。
- 源码结构(这是好消息):渲染/事件/音频都做了抽象层
  ```
  src/main.cpp
  src/app.cpp|h                  ← 应用入口
  src/app_state/…                ← 菜单 / 关卡选择 / 游戏状态机 / 计分
  src/engine/engine.h, renderer.h, sound_manager.h   ← 抽象接口
  src/engine/sdl/sdl_engine.cpp
  src/engine/sdl/sdl_renderer.cpp       ← SDL_Renderer(Android 上自动走 GLES)
  src/engine/sdl/sdl_event_mapper.h     ← 键盘事件 → 游戏动作
  src/engine/sdl/sdl_sound_manager.cpp  ← SDL_mixer
  src/objects/…                         ← 坦克 / 砖墙 / 鹰 / 子弹 / 道具
  ```
  游戏逻辑只依赖 `engine/*.h`,**不需要改玩法代码**。
- 资源:`resources/img/*.png`、`resources/sounds/*.ogg`、`resources/stages/0..35`(文本关卡)、`resources/font/kongtext.ttf`、`resources/textures/texture.png`。

**本方案的工作量集中在三处**:①NDK 重写构建;②资源从 APK assets 解包;③输入适配。**最大雷区是 SDL2_ttf 2.24(强依赖 freetype+harfbuzz)**——本方案在阶段 G 给出了绕过办法,别一开始就啃它。

**战略原则:先让画面出来,再管声音和文字。** 顺序错了会很痛苦。

---

## 1. 目录规划

```
TankTv/
├── ANDROID_PORT_PLAN.md        ← 本文档
├── .gitignore
├── app/                        ← 整份拷贝自 SDL 的 android-project(Gradle 根)
│   ├── build.gradle
│   ├── settings.gradle
│   ├── gradlew
│   └── app/                    ← ⚠️ 模块目录,注意与上一层重名
│       ├── build.gradle
│       ├── src/main/AndroidManifest.xml
│       ├── src/main/assets/resources/   ← 游戏资源(从游戏仓库拷来)
│       └── jni/  (或 src/main/jni/)     ← NDK 构建脚本,路径视 SDL 版本而定
├── third_party/                ← 第三方库源码,不进版本库
│   ├── SDL/                    ← SDL2 2.32.4
│   ├── SDL_image/              ← 可选
│   └── SDL_mixer/              ← 可选
└── game/                       ← krystiankaluzny/Tanks 克隆(含 src/ 和 resources/)
```

> ⚠️ **`app/app/` 这个双层同名目录是 SDL 模板的原始结构,不要试图拍平**,否则按官方文档改 Gradle 时会到处对不上。下文所有路径都按此写法。

> ⚠️ **路径必须是纯 ASCII,不要有空格和中文**(NDK/CMake 对此非常敏感)。`E:\AndroidProject\TankTv` 目前合规,别改。

`.gitignore` 建议内容:

```gitignore
third_party/
build/
.gradle/
local.properties
*.iml
.idea/
.cxx/
```

---

## 2. 阶段 0:环境准备

> ✅ **本机已完成,无需重装。** 实测到的版本写在第 B 节「实际执行记录」里。
> 只有换到**另一台电脑**时才需重新走本节。

### 2.1 软件清单

| 软件 | 版本要求 | 备注 |
|---|---|---|
| JDK | **17**(AGP 8.x 要求) | 装 Android Studio 自带的那个即可 |
| Android Studio | 最新稳定版 | 只为拿 SDK/NDK/CMake,不一定用它开发 |
| Android SDK Platform | API 34 或 35 | |
| **NDK** | **r26d / r27 LTS** | 本机用的是 `android-ndk-r26d`,实测 SDL2 2.32.4 编译通过 |
| **CMake** | **3.22.1**(SDK Manager 里那个版本) | 与 AGP 匹配 |
| Git | 任意 | |
| MSYS2 | 可选,仅阶段 A 用 | 见阶段 A |
| 设备 | 一台 Android 手机或 TV,开启 USB 调试 | 或模拟器(但 TV 一定要真机验遥控器) |

### 2.2 安装命令(Windows)

在 Android Studio 里:`Settings → Languages & Frameworks → Android SDK → SDK Tools`,勾选:

- `NDK (Side by side)` → 选 **27.x**
- `CMake` → 选 **3.22.1**

命令行等价(装了 cmdline-tools 才有):

```sh
cd /e/AndroidProject/TankTv
sdkmanager --install "ndk;27.2.12479018" "cmake;3.22.1" "platforms;android-35"
```

> 版本号如已更新,用 `sdkmanager --list | grep -i "ndk;"` 查最新 LTS。

### 2.3 验收

```sh
java -version          # 期望:17.x
adb devices            # 期望:列出你的设备,状态 device
echo %ANDROID_HOME%    # 期望:能打印路径(或 ANDROID_SDK_ROOT)
```

> 在 Git Bash / `sh` 下用 `echo $ANDROID_HOME`。

### 2.4 换电脑后的「恢复检查」

```sh
cd /e/AndroidProject/TankTv
git status                        # 看上次停在哪
ls third_party/SDL                # 有 → 阶段 B 的源码已拉
ls app/app                        # 有 → 模板已拷入
find app -name CMakeLists.txt     # 有 → 构建脚本就位
ls app/app/src/main/assets/resources # 有 → 阶段 D 已完成
```

然后对照第 P 节的进度看板继续。

---

## 2B. 本机实际执行记录(2026-11-04)

> 这一节记录**真实跑过的命令、真实遇到的报错和真实修法**。换电脑后先看这里,
> 80% 的坑会重复出现。

### 2B.1 环境实测结果

```
git        2.55.0.windows.5
java       17.0.10 (D:/Program Files/Java/jdk-17)
adb        D:/Sdk/platform-tools/adb
SDK        D:/Sdk      → platforms: 28,30,31,32,33,34,35,36 ; build-tools: 到 36.0.0
NDK        D:/Sdk/ndk/android-ndk-r26d   (还有 28.2.13676358 / android-ndk-r29)
CMake      D:/Sdk/cmake/3.22.1           (还有 3.10.2 / 3.18.1)
GRADLE_USER_HOME = D:/.gradle      ← 注意不在用户目录!
OS 默认 shell = sh (Git Bash),项目路径写作 /e/AndroidProject/TankTv
```

### 2B.2 坑 1:GitHub 直连被封

**现象**:
- `git clone https://github.com/...` → `Connection timed out after 300025 milliseconds`
- `curl -I https://github.com` → `http=000`

**但下面这些是通的**(所以问题只在 github.com 的 git 协议/网页):

| 主机 | 结果 |
|---|---|
| api.github.com | ✅ 200 |
| **codeload.github.com** | ✅ 200(**用它下 zip**)|
| raw.githubusercontent.com | ✅ 被封(000)|
| services.gradle.org | ✅ 307(但重定向到 github,所以没用)|
| dl.google.com | ✅ 200 |
| repo1.maven.org | ✅ 200 |
| 腾讯/华为 gradle 镜像 | ✅ 200 |

**解决办法**:不走 git,改用 codeload 下 zip:

```sh
cd /e/AndroidProject/TankTv
curl -L --retry 3 -o /tmp/Tanks.zip \
  https://codeload.github.com/krystiankaluzny/Tanks/zip/refs/heads/master
unzip -q /tmp/Tanks.zip -d /tmp/tanksx && mv /tmp/tanksx/Tanks-master game
```

> 实测:zip 只有 **885KB / 177 个文件**。仓库里那些 Windows DLL 并不在 zip 里(反而正好)。
> SDL 源码是用 git 拉的(当时网络尚可),但如果拉不下来,同样可以用 codeload:
> `https://codeload.github.com/libsdl-org/SDL/zip/refs/tags/release-2.32.4`

### 2B.3 坑 2:Gradle 被写死的死代理卡住

**现象**:`Exception in thread "main" java.net.ConnectException: Connection refused`,
Gradle 报 `Connect to 127.0.0.1:7890 [/127.0.0.1] failed`。

**原因**:`D:/.gradle/gradle.properties` 里有:

```properties
systemProp.http.proxyHost=127.0.0.1
systemProp.http.proxyPort=7890
systemProp.https.proxyHost=127.0.0.1
systemProp.https.proxyPort=7890
```

代理工具(Clash 类)没在运行时,Gradle 就全挂。注意 Windows 系统代理里 `ProxyEnable=0`,所以
curl 能直连、只有 Gradle 挂了——很容易误判成「网络问题」。

**解决办法**(任选):
1. **把代理工具打开**(最简单,打开后 github 也能通)
2. 构建时临时禁用(本次用的这个,已还原):
   ```sh
   mv /d/.gradle/gradle.properties /d/.gradle/gradle.properties.off
   # ... 构建 ...
   mv /d/.gradle/gradle.properties.off /d/.gradle/gradle.properties
   ```

### 2B.4 坑 3:Gradle 发行包下载不了 → 用镜像 + 跳过 wrapper

`services.gradle.org` 的 307 重定向指向 **github.com**(被封):

```
Location: https://github.com/gradle/gradle-distributions/releases/download/v8.1.1/gradle-8.1.1-bin.zip
```

所以 `./gradlew` 永远卡在下载。解法:用腾讯云镜像下好,直接跑那个 gradle:

```sh
mkdir -p /d/tools
curl -L -o /d/tools/gradle-8.1.1-bin.zip \
  https://mirrors.cloud.tencent.com/gradle/gradle-8.1.1-bin.zip
unzip -q -o /d/tools/gradle-8.1.1-bin.zip -d /d/tools
```

实测:**124MB / 14 秒 / 8.9MB/s**。之后所有构建都用:

```sh
cd /e/AndroidProject/TankTv/app
/d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon --console=plain
```

> 华为云镜像也通:`https://mirrors.huaweicloud.com/gradle/gradle-8.1.1-bin.zip`
> 阿里云那个是 404。

### 2B.5 模板真相:它默认用 ndk-build,不是 CMake

SDL 2.32.4 的 `android-project`:

```
app/
├── build.gradle              ← AGP 8.1.1,wrapper 用 gradle-8.1.1
└── app/                      ← 模块(注意双层同名)
    ├── build.gradle          ← 默认开的是 ndkBuild,cmake 分支是注释的
    └── jni/
        ├── Android.mk        ← 内容仅:include $(call all-subdir-makefiles)
        ├── Application.mk    ← APP_PLATFORM=android-16(太老,必须改)
        ├── CMakeLists.txt    ← add_subdirectory(SDL) ← 写死子目录!
        └── src/
            ├── Android.mk    ← SDL_PATH := ../SDL ← 也写死!
            └── CMakeLists.txt
```

**关键**:两种构建方式都要求 SDL 源码在 `app/jni/SDL`。
本项目的做法是**改成 CMake + 绝对/相对路径**,不去复制那 90MB:

```cmake
# app/app/jni/CMakeLists.txt
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party")
add_subdirectory("${THIRD_PARTY_DIR}/SDL" SDL)
```

> 路径层级:`TankTv/app/app/jni` 往上是 **3 层**到 `TankTv/`。第一次我写成 4 层,侥幸在
> `if(NOT EXISTS ...)` 的检查里报错才发现——改路径后一定要看 CMake 的 FATAL_ERROR 提示。

### 2B.6 坑 4:`applicationId` 放错块

**报错**:`Could not find method applicationId() for arguments [...] on extension 'android'`

**原因**:`applicationId` 必须写在 `defaultConfig {}` 里,不能直接写在 `android {}` 里。

### 2B.7 坑 5:SDL2 的 Keysym 没有 `keycode` 字段

**报错**:`error: no member named 'keycode' in 'struct SDL_Keysym'`

**原因**:SDL2 里 `SDL_Keysym` 只有 `scancode` / `sym` / `mod`。
`sym` 才是 keycode。(`keycode` 这种写法只在某些旧代码/误解里存在)

### 2B.8 阶段 B 最终验证状态

```
BUILD SUCCESSFUL
app/app/build/outputs/apk/debug/app-debug.apk   1.2 MB

APK 内容:
  lib/arm64-v8a/libSDL2.so       2.0 MB
  lib/arm64-v8a/libmain.so       6.6 KB
  lib/arm64-v8a/libc++_shared.so 1.3 MB
```

**尚未验证**:还没装到设备上跑过(执行时 `adb devices` 为空)。
→ 这是阶段 B 剩下的唯一一步,见下面「卡住的退路」上方的验收命令。

---

## 3. 阶段 A:桌面版基线(可选,但强烈建议)

**为什么做**:先用 30 分钟确认「这份源码本身能正常跑」,后面 Android 上出问题时才能确定是移植的问题而不是代码的问题。

**代价**:要装 MSYS2+MinGW。**嫌麻烦可跳过**,直接进阶段 B;但跳过意味着你失去了唯一的对照组。

```sh
# MSYS2 里执行
pacman -S mingw-w64-x86_64-gcc make pkg-config \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image \
          mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-SDL2_ttf
```

```sh
cd /e/AndroidProject/TankTv/game
cat Makefile            # 看编译器/链接参数/std 版本,后面写 CMakeLists 要抄
make -j4
```

**验收**:能打开游戏窗口,进入菜单,方向键+空格能开一局。

**顺手记录**(写进本文档末尾的「侦察结果」):
- Makefile 里的 `-std=c++XX` 是什么
- 编译了哪些 `.cpp`(是否自动 glob)
- 链接了哪些库

> 如果 make 报错且不好修:跳过阶段 A,但把报错贴给我。

---

## 4. 阶段 B:先让 SDL2 模板跑出一个 APK

**目标**:屏幕上出现一个纯色/清屏画面。此时**还没有任何游戏代码**。这一步只验证工具链。

### 4.1 拉源码

```sh
cd /e/AndroidProject/TankTv
mkdir -p third_party
git clone --depth 1 --branch release-2.32.4 https://github.com/libsdl-org/SDL.git third_party/SDL
# tag 不存在就去掉 --branch 用默认分支,或 ls-remote 查一下:
#   git ls-remote --tags https://github.com/libsdl-org/SDL.git | grep 2.32
```

同时把游戏源码也拉下来(现在拉,后面不用再等):

```sh
git clone --depth 1 https://github.com/krystiankaluzny/Tanks.git game
```

### 4.2 复制官方模板

```sh
cp -r third_party/SDL/android-project ./app
ls app            # 期望看到:app/ build.gradle settings.gradle gradlew …(Gradle 根)
ls app/app        # 期望看到:build.gradle src/(这就是 Android 模块)
```

### 4.3 找到并修改构建路径(不要凭记忆猜路径,用命令找)

```sh
find app -name "CMakeLists.txt"
grep -rn "cmake" app/app/build.gradle        # 看 externalNativeBuild 指向哪个文件
```

打开那个 `CMakeLists.txt`,把里面指向 SDL 源码的变量(通常叫 `SDL2_SOURCE_DIR` / `SDL_DIR`,模板里是注释掉或写死的相对路径)改成:

```cmake
set(SDL2_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../third_party/SDL")
```

> 相对层级视模板所在位置而定,**用 `ls` 逐层验证到 `third_party/SDL` 存在**,不要照抄。

再检查 `app/app/build.gradle` 里的:
- `applicationId`(随便改成 `com.example.tanktv`)
- `minSdk` → 21
- `targetSdk` / `compileSdk` → 34 或 35
- `ndkVersion` → 你装的版本

### 4.4 构建

> ✅ **本节已在本机完成并验证。下面写的是「修正后的实际做法」。**
> ⚠️ 所有 `sh` 代码块都以绝对 `cd` 开头,可整块复制。
> **换了电脑/盘符,把 `/e/AndroidProject/TankTv` 换成实际路径**(Git Bash 里 `D:\xxx` 写作 `/d/xxx`)。

先写 `app/local.properties`(注意是 **`TankTv/app/`** 这一层):

```properties
sdk.dir=D\:\\Sdk
cmake.dir=D\:\\Sdk\\cmake\\3.22.1
```

再改 `app/app/build.gradle`(模块层):

| 项 | 改成 | 说明 |
|---|---|---|
| `namespace` | `"org.libsdl.app"` | 保持不动(Java 里的包名)|
| `defaultConfig.applicationId` | `"com.example.tanktv"` | **必须写在 defaultConfig 里** |
| `minSdkVersion` | `21` | 模板的 19 太老 |
| `targetSdkVersion` / `compileSdkVersion` | `34` | 本机已装 |
| `ndkPath` | `'D:/Sdk/ndk/android-ndk-r26d'` | 用 `ndkPath`,**不要用 local.properties 的 `ndk.dir`**(已弃用,会报 CXX5106)|
| 构建后端 | 注释掉 `ndkBuild`,启用 `cmake` | 模板默认是 ndk-build |
| `abiFilters` | 先只留 `'arm64-v8a'` | 编译快 4 倍 |

构建命令(**注意不是 `./gradlew`,见 2B.4**):

```sh
cd /e/AndroidProject/TankTv/app
/d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon --console=plain
ls -lh app/build/outputs/apk/debug/
```

**验收**

```sh
cd /e/AndroidProject/TankTv
ls app/app/build/outputs/apk/debug/app-debug.apk       # 先确认产物存在(不依赖设备)
unzip -l app/app/build/outputs/apk/debug/app-debug.apk | grep lib/
# 期望:libSDL2.so  libmain.so  libc++_shared.so

# 有设备时才需要下面这些
adb devices
adb install -r app/app/build/outputs/apk/debug/app-debug.apk
adb shell cmd package resolve-activity --brief com.example.tanktv
adb shell am start -n com.example.tanktv/org.libsdl.app.SDLActivity
adb logcat | grep -iE "SDL|TankTv"
```

期望日志:

```
=== TankTv bootstrap ===
SDL version: 2.32.4
renderer: OpenGL ES 2.0
```

屏幕上应当看到**缓慢渐变的蓝底**(颜色一直在动 = 真的在逐帧刷新)。
此时按遥控器方向键/OK,logcat 会打出 `KEYDOWN scancode=.. sym=..` —— **这就是阶段 F 需要的键位数据,先把它抄下来。**

**卡住时的退路**
- `Connection refused` / `Connect to 127.0.0.1:7890` → 代理坑,见 2B.3。
- Gradle 下载卡住 → 见 2B.4,用镜像。
- `SDL 源码不存在: .../third_party/SDL` → CMakeLists 里的相对层级错了,见 2B.5。
- `Could not find method applicationId()` → 见 2B.6。
- `NDK not configured` → 检查 `app/app/build.gradle` 里的 `ndkPath` 是否真实存在。

---

## 5. 阶段 C:把游戏源码编进去(无资源、无声音、无文字)

**目标**:APK 里跑的是 `Tanks` 的代码,能走到主循环。此时屏幕可能还是黑的或者崩在加载资源,正常。

### 5.1 先把 `main()` 接起来

✅ **已侦察清楚**:游戏入口是 `game/src/main.cpp`,内容极简:

```cpp
#include "app.h"

int main(int argc, char* args[])
{
    App app;
    app.run();
    return 0;
}
```

它**没有** `#include <SDL.h>`——SDL 的头在 `app.h` 链里。所以正确做法不是「把 main 搬进 main.c」,
而是:直接把这个 `.cpp` 当普通源文件编进去,然后**删掉引导程序 `main.c`**(它也有一个 `main`,会重复定义)。

### 5.2 把源码加进 CMake

✅ **已侦察**:Makefile 用 `-std=c++17`,`-Wno-narrowing`,并把所有 `.cpp` 自动 glob(含子目录)。

改 `app/app/jni/src/CMakeLists.txt`,把 `main.c` 换成游戏源码(**不要手写文件列表**,
也不要用 `file(GLOB ... "**/*.cpp")` —— `**` 在 `GLOB` 里不生效,必须 `GLOB_RECURSE`):

```cmake
# 文件位置: TankTv/app/app/jni/src/CMakeLists.txt
# 所以往上是 4 层才到 TankTv/(src → jni → app → app → TankTv)
set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../game")

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
target_sources(main PRIVATE ${GAME_SOURCES})

target_include_directories(main PRIVATE "${GAME_DIR}/src")
```

> 验证递归 glob 有没有生效(应该几十个文件,而不是 1 个):
> ```cmake
> list(LENGTH GAME_SOURCES _n)
> message(STATUS "game sources: ${_n}")
> ```

编译选项(先按阶段 A 的侦察结果抄):

```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

**这一步只链接 SDL2**(先不要 image/mixer/ttf):

```cmake
target_link_libraries(main SDL2)
```

> 注意:模板里写的是 `find_library(SDL2 SDL2)` + `target_link_libraries(main SDL2)`,
> 因为我们用 `add_subdirectory` 引入 SDL,直接用 target 名 `SDL2` 即可(已实测可行)。
>
> 此时 `IMG_Load` / `Mix_` / `TTF_` 全是 undefined symbol 是**预期内的**。先看别的错。

### 5.3 验收

```sh
cd /e/AndroidProject/TankTv/app
/d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon --console=plain 2>&1 | tail -30
```

**通过标准**:能编出 APK,启动后不因 undefined symbol 失败,日志里能看到游戏自己的输出(哪怕紧接着因为找不到资源而 abort)。

### 5.4 阶段 C 的实际待办清单(下次从这里开始)

1. **删掉** `app/app/jni/src/main.c`(引导程序,会和游戏的 `main` 冲突)
2. 改 `app/app/jni/src/CMakeLists.txt`:
   ```cmake
   file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
   target_sources(main PRIVATE ${GAME_SOURCES})
   target_include_directories(main PRIVATE "${GAME_DIR}/src")
   ```
   `GAME_DIR = ${CMAKE_CURRENT_SOURCE_DIR}/../../../../game`(4 层,已核对)
3. 保留 `CXX_STANDARD 17` + 加 `-Wno-narrowing`(游戏代码在 gcc 下需要它)
4. 构建。预期会出现大量 `undefined reference to IMG_Load / Mix_ / TTF_`
5. **两种走法**(按你想要的进度选):
   - **快路**:先编 SDL_image(只要它),让贴图能加载;声音/文字先 stub 掉
   - **完整路**:依次把 SDL_image / SDL_mixer / SDL_ttf 都用 `add_subdirectory` 加进来
6. 如果编译报 C++ 语法错(比如 `narrowing`),先看 `-Wno-narrowing` 是否生效

**预判的编译雷区**(从代码风格看):
- 用了 C++17 的 lambda 初始化 (`inline vector<KeyCode> v = []{...}()`),必须真的开 `-std=c++17`
- `using namespace std;` 在头文件里 → 容易和 NDK 的符号冲突,如有重定义错误优先怀疑这里

### 5.4 快速定位「资源缺失」类崩溃

```sh
grep -rn "resources/" game/src/ | head -40
grep -rn "IMG_Load\|Mix_Load\|TTF_Open" game/src/ | head -40
grep -rn "fopen\|ifstream\|ofstream" game/src/ | head -30
```

把输出抄进本文档末尾的「侦察结果」。这份清单决定阶段 D/E/G 要改几处。

---

## 6. 阶段 D:资源解包(最影响观感的一步)

**问题**:APK 里的 `assets/` 是只读压缩包,C 代码用相对路径必然读不到。

✅ **已侦察清楚资源结构**(修正了本文档早先的猜测):游戏运行时需要的是**扁平结构**:

```
texture.png      ← 来自 resources/textures/texture.png
kongtext.ttf     ← 来自 resources/font/kongtext.ttf
stages/0..35     ← 原样
sounds/*.ogg     ← 原样
```

> ⚠️ `resources/img/*.png` **不被使用**(游戏用单张 `texture.png` 图集切图)。
> 这是从 `appconfig.h` 的 `texture_path` 和 Makefile 的 `APP_RESOURCES` 交叉验证出来的。

**方案:启动时把上述文件解到内部存储的一个**扁平**目录,然后 `chdir` 过去。**
这样游戏代码里的相对路径**一行都不用改**。

### 6.1 放资源

按实测的结构放(注意目标文件名是**扁平**的):

```sh
cd /e/AndroidProject/TankTv
A=app/app/src/main/assets
mkdir -p $A/stages $A/sounds
cp game/resources/textures/texture.png  $A/texture.png
cp game/resources/font/kongtext.ttf     $A/kongtext.ttf
cp game/resources/stages/*              $A/stages/
cp game/resources/sounds/*.ogg          $A/sounds/
du -sh $A
```

> 上面用了 `$A` 变量。若你的 shell 不允许变量,直接展开写全路径即可。
> 体积参考:`game/resources` 总共约 **1.2MB**,进 APK 完全无压力。

### 6.2 写解包器(Java,子类化 SDLActivity)

新建 `app/app/src/main/java/com/example/tanktv/GameActivity.java`(包名与 `applicationId` 一致):

```java
package com.example.tanktv;

import android.content.res.AssetManager;
import java.io.*;
import org.libsdl.app.SDLActivity;

public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(android.os.Bundle b) {
        extractAssets();
        super.onCreate(b);
    }

    private void extractAssets() {
        // 注意:目标是 filesDir 根下(扁平),不是 filesDir/resources
        File dst = getFilesDir();
        if (new File(dst, "texture.png").exists()) return;   // 简单幂等
        try {
            copyDir(getAssets(), "", dst);
        } catch (IOException e) {
            android.util.Log.e("TankTv", "extract failed", e);
        }
    }

    private static void copyDir(AssetManager am, String src, File dst) throws IOException {
        String[] list = am.list(src);
        if (list != null && list.length > 0) {
            dst.mkdirs();
            for (String name : list) {
                String child = src.isEmpty() ? name : src + "/" + name;
                copyDir(am, child, new File(dst, name));
            }
        } else {
            dst.getParentFile().mkdirs();
            try (InputStream in = am.open(src);
                 OutputStream out = new FileOutputStream(dst)) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            }
        }
    }
}
```

改 `app/app/src/main/AndroidManifest.xml`,把 Activity 从 `SDLActivity` 改成你自己这个:

```xml
<activity android:name=".GameActivity" ... >
```

### 6.3 native 侧切目录

在游戏 `main()` **最开始**(初始化 engine 之前)加:

```c
#ifdef __ANDROID__
#include <unistd.h>
#include <SDL_system.h>
    chdir(SDL_AndroidGetInternalStoragePath());   // 在初始化 engine 之前
#endif
```

> `SDL_AndroidGetInternalStoragePath()` 在 SDL 的实现里对应 Java 侧的 `getFilesDir()`,也就是 6.2 解包的目标目录,**两者应当一致**。但这一点依赖 SDL 版本,**务必实测验证**,不要假设:
>
> ```c
> #ifdef __ANDROID__
>     SDL_Log("internal=%s", SDL_AndroidGetInternalStoragePath());
>     char cwd[512]; getcwd(cwd, sizeof cwd);
>     SDL_Log("cwd=%s", cwd);
> #endif
> ```
>
> 两个路径不同 → 把 6.2 的解包目标改成 native 报告的那个目录;或反过来,Java 侧用 `getFilesDir()` 之外的值。日志里能看到 `internal=...` 就直接抄那个路径。

### 6.4 验收

启动后:**能看到地图、坦克、砖墙**。文字可能是空白方块(阶段 G),没有声音(阶段 E)。

---

## 7. 阶段 E:声音(可往后放)

**目标**:证明 `SDL_mixer` 在 Android 上可用。

**做法(CMake 优先)**:

```sh
git clone --depth 1 --branch release-2.8.1 https://github.com/libsdl-org/SDL_mixer.git third_party/SDL_mixer
```

在 `CMakeLists.txt` 里(相对层级同样要自己验证):

```cmake
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/SDL_mixer" EXCLUDE_FROM_ALL)
target_link_libraries(main SDL2_mixer::SDL2_mixer)
```

> SDL_mixer 2.8 的 CMake 在 Android 上会尝试构建 `external/` 下的编解码库。**这是本方案第二容易翻车的地方**,所以放在阶段 D 之后——即使失败也不影响你玩。

**验收**:有射击声、爆炸声。

**退路(都很实用)**
1. **注意:这游戏的音效全是 `.ogg`(Vorbis 编码)**,所以 Vorbis 解码器省不掉,开关里别把 vorbis 关了。
2. 不想要这套依赖:把 `resources/sounds/*.ogg` 批量转成 **WAV**(ffmpeg 一条命令),体积变大但可以只要最基本的解码路径。
3. 用 `ndk-build` 编译 mixer:它的源码树自带 `external/libogg`、`external/libvorbis` 和 `Android.mk`。
4. 全都失败:**先放着**,游戏照样能玩,后续再补。

---

## 8. 阶段 F:输入适配

### 8.1 先查游戏用了哪些键

```sh
grep -rn "SDL_SCANCODE" game/src/ | sort -u
cat game/src/engine/data/keycode.h          # ← 游戏自己的键位枚举,必看
sed -n '1,120p' game/src/engine/sdl/sdl_event_mapper.h
```

记下:上/下/左/右/开火/暂停/确认 分别映射到哪个 `SDL_SCANCODE`。

### 8.2 Android TV 遥控器(几乎零成本)

SDL2 的 `SDLActivity` 会把 `AKEYCODE_DPAD_UP/DOWN/LEFT/RIGHT` 转成 `SDL_SCANCODE_UP/DOWN/LEFT/RIGHT`。TV 上大概率**直接能玩**。

「OK 键」映射成哪个 scancode 各版本不同,先打日志确认:

```c
case SDL_KEYDOWN:
    SDL_Log("KEY scancode=%d keycode=%d", e.key.keysym.scancode, e.key.keysym.keycode);
```

若 OK 没反应,在 `sdl_event_mapper.h` 里把 `SDL_SCANCODE_KP_ENTER` / `SDL_SCANCODE_RETURN` / `SDL_SCANCODE_SPACE` 都当作「确认」即可(用 `#ifdef __ANDROID__` 包住,不影响桌面版)。

### 8.3 手机触摸(需要写代码)

最小实现:用 SDL 自带的虚拟摇杆?没有。两种做法:

- **A. Java 覆盖层**(推荐,改动小):在 `GameActivity` 里加四个按钮 + 一个开火键,按下/抬起时调用 SDL 的
  ```java
  SDLActivity.onNativeKeyDown(android.view.KeyEvent.KEYCODE_DPAD_UP);
  SDLActivity.onNativeKeyUp(android.view.KeyEvent.KEYCODE_DPAD_UP);
  ```
  等于「让触摸冒充遥控器」,native 侧零改动。
- **B. native 侧读 `SDL_FINGERDOWN/UP`**,自己维护虚拟按键状态。更灵活,但要改 `sdl_event_mapper`。

> 方法名以你手上 SDL 版本的 `SDLActivity.java` 为准(搜 `onNativeKeyDown`)。

**验收**:TV 遥控器打通一关;手机上能移动+开火。

---

## 9. 阶段 G:文字(故意放最后)

**背景**:游戏用 `SDL2_ttf 2.24` 渲染文字,而它强依赖 freetype + harfbuzz,Android 交叉编译最烦。**而你的文字需求只有:菜单项、分数数字、GAME OVER。** 所以有三档选择:

| 方案 | 成本 | 适用 |
|---|---|---|
| **G1 干脆不显示文字** | 0 | 能玩,丑 |
| **G2 `stb_truetype.h` 单头文件渲染 TTF** | 低 | **推荐**。放一个头文件进 `third_party/`,用现有的 `kongtext.ttf`,替换 `sdl_renderer` 里那几处 ttf 调用 |
| G3 真编 SDL2_ttf + freetype + harfbuzz | 高 | 只有你确定要复用原渲染代码时 |

先看现状:

```sh
grep -rn "TTF_" game/src/
```

通常就 3~5 处(打开字体、渲染文本、取文本尺寸、释放)。G2 就是把这 5 处换掉。

**验收**:菜单文字、分数正常显示。

---

## 10. 阶段 H:存档 / 最高分

```sh
grep -rn "fopen\|ofstream\|ifstream\|highscore" game/src/
```

- 如果它**写在自己工作目录**(阶段 D 已 chdir 到内部存储):**直接就能用**,因为那是可写目录。
- 如果它写到别的固定路径:改成 `SDL_GetPrefPath("", "Tanks")`。

**验收**:玩一局 → 杀进程 → 重开,最高分还在。

---

## 11. 阶段 I:分辨率与全屏

```sh
grep -rn "SDL_CreateWindow\|SDL_CreateRenderer\|WIDTH\|HEIGHT" game/src/appconfig.h game/src/engine/sdl/
```

Android 上通常是「手机竖屏设备但有横屏游戏」,处理:

```c
// sdl_renderer.cpp 里 CreateRenderer 之后
SDL_RenderSetLogicalSize(renderer, LOGICAL_W, LOGICAL_H);   // 自动等比 + 黑边
SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);     // 或创建时就用 FULLSCREEN
```

另外在 `AndroidManifest.xml` 的 Activity 上锁横屏:

```xml
android:screenOrientation="landscape"
```

用 `#ifdef __ANDROID__` 包住,保证桌面版不受影响。

**验收**:画面居中、比例正确、无拉伸;黑边可以接受。

---

## 12. 阶段 J:做成合格的 Android TV 应用

`AndroidManifest.xml` 补充:

```xml
<uses-feature android:name="android.hardware.touchscreen" android:required="false" />
<uses-feature android:name="android.software.leanback" android:required="false" />

<!-- 在 launcher activity 里加 TV 入口 -->
<intent-filter>
    <action android:name="android.intent.action.MAIN" />
    <category android:name="android.intent.category.LAUNCHER" />
    <category android:name="android.intent.category.LEANBACK_LAUNCHER" />
</intent-filter>
```

- TV 需要 `banner`(320×180)图标,否则电视上不显示:`android:banner="@drawable/tv_banner"`。
- 应用名/图标换成坦克。
- `targetSdk` 别低于 34,否则新 TV 会有兼容模式。

```sh
cd /e/AndroidProject/TankTv/app && ./gradlew assembleDebug
cd /e/AndroidProject/TankTv
adb install -r app/app/build/outputs/apk/debug/app-debug.apk
```

**验收**:TV 首页出现你的应用卡片(有 banner),点开能玩。

---

## 13. 时间预算与「止损门」

| 阶段 | 内容 | 乐观 | 现实 | 实际 |
|---|---|---|---|---|
| 0 | 环境 | 0.5h | 1h | ✅ **0h**(本机已有,无需安装)|
| A | 桌面基线(可选) | 0.5h | 1.5h | ⏭️ **跳过** |
| B | 模板 APK | 1h | 2.5h | ✅ **~40min**(含下 Gradle/排网络坑,构建本身 2.5min)|
| C | 编入源码 | 1h | 4h | ⬜ 预计 1~3h |
| **门 1** | **画面能出** | — | — | — |
| D | 资源解包 | 1h | 2h | 已预研,预计 1h |
| E | 声音 | 1h | 3h(可放弃) | 已预研,预计 1~3h |
| F | 输入 | 1h | 2.5h | TV 遥控器预计 30min |
| G | 文字 | 0.5h | 2h | 用 stb_truetype 预计 1h |
| H/I/J | 收尾 | 1h | 2h | — |
| 合计 | | **~7h** | **~2 天** | 已完成约 1/3 |

**门 1(阶段 C 结束)是唯一的关键判断点**:

- 通过 → 后面都是体力活,继续。
- 卡满 **6 小时**仍编不出能启动的 APK → 停止,转用「方案 A/B」(Winlator / WebView 包装网页版)。这不是失败,是及时止损。

---

## 14. 常见错误速查

| 现象 | 原因 | 处理 |
|---|---|---|
| `SDL2 not found` | `SDL2_SOURCE_DIR` 路径错 | 回到 4.3,逐层 ls 验证 |
| `undefined reference to IMG_Load` | 还没加 SDL2_image | 预期,先看别的错 |
| `undefined symbol: TTF_Init` | 还没做阶段 G(文字) | 预期,先放着 |
| 启动即闪退,`AndroidRuntime: FATAL` | Java 侧异常 | `adb logcat -s AndroidRuntime` 看首行异常 |
| 黑屏但进程活着 | `SDL_main` 没接对 / 没进主循环 | 在 main 里 `SDL_Log("alive")` 打点 |
| 一堆 `Could not load resources/...` | 阶段 D 没做或 chdir 没生效 | 打 `getcwd` 日志 |
| 中文/空格路径报奇怪错 | NDK 路径限制 | 项目挪到纯 ASCII 路径 |
| `libmain.so` 找不到 | CMake target 名与 Java 里 `getLibraries()` 不一致 | 两处对齐 |
| 触摸坐标偏了 | 逻辑分辨率缩放过 | 用 `SDL_RenderSetLogicalSize` 后坐标会被自动换算 |
| TV 上不显示图标 | 缺 banner | 阶段 J |

---

## 15. 侦察结果(2026-11-04 已实测填写)

```
[✓] Makefile 的 C++ 标准:            -std=c++17(Linux/Mac 分支);另有 -Wno-narrowing
[✓] 游戏 main() 的签名:               int main(int argc, char* args[]);只做 App app; app.run();
[✓] 资源路径写法(appconfig.h):       texture_path = "texture.png"
                                      stages_path  = "stages/"
                                      font_path    = "kongtext.ttf"
                                      声音路径见 soundconfig.h(需再确认)
[✓] 资源目录真相:                    Makefile 只拷 textures/texture.png、font/kongtext.ttf、
                                      stages、sounds 到运行目录。
                                      ⚠️ resources/img/ 根本没被使用(游戏用单张图集切图)
[ ] IMG_Load 调用处数量:              待查
[ ] TTF_ 调用处:                      待查
[ ] Mix_ 调用处:                      待查
[ ] 写文件(高分)的方式:             待查(有 high_score.h / scores.cpp)
[✓] 窗口逻辑分辨率:                   464 x 416
                                      map 26x26 tiles x 16px = 416x416,status 3x16 = 48px
[✓] 玩家键位(appconfig.h):          P1 = ↑↓←→ + 右Ctrl(开火)
                                      P2 = W S A D + 左Ctrl
                                      ⚠️ 没有独立暂停键
[✓] 游戏完整度:                       stages_count = 35,每关 20 敌人(完整版)
[✓] APK 包名:                         com.example.tanktv
[✓] Java 内 SDL 库名列表:            "SDL2", "main"(模板默认)
[ ] 遥控器方向键/OK 的 scancode:      需上设备后从 logcat 抄(引导程序已埋日志)
[ ] Makefile 实际链接库:             SDL2main SDL2 SDL2_image SDL2_ttf SDL2_mixer
```

> **对阶段 D 的影响(重要)**:解包后的目录结构应该是
> ```
> <internal>/              ← chdir 到这里
> ├── texture.png          ← 来自 resources/textures/texture.png
> ├── kongtext.ttf         ← 来自 resources/font/kongtext.ttf
> ├── stages/0..35         ← 原样
> └── sounds/*.ogg         ← 原样
> ```
> **不是** `resources/img/...`。文档第 6 节的示例要按这个改。

---

## 16. 参考资料

| 用途 | 链接 |
|---|---|
| 游戏源码(MIT) | https://github.com/krystiankaluzny/Tanks |
| SDL2 官方(含 `android-project` 模板) | https://github.com/libsdl-org/SDL |
| SDL2 Android 官方文档 | https://wiki.libsdl.org/SDL2/README-android |
| SDL_image / SDL_mixer / SDL_ttf | `libsdl-org/SDL_image`、`SDL_mixer`、`SDL_ttf` |
| 通用「SDL 游戏→APK」脚手架(580★,可抄它的输入/解包实现) | https://github.com/pelya/commandergenius |
| stb_truetype(单头文件 TTF 渲染) | https://github.com/nothings/stb |

---

## 17. 备选路线(只写在附录里,别一开始就想)

- **A. Winlator**(Android 上的 Wine+Box64):直接跑 Windows 版 exe,零改动,兼容性看运气。
- **B. WebView 包一层** `https://fc-tank.bencao.it`(JS/MIT 复刻):半小时出可玩 APK,但不是这份代码。

**判断标准**:你想「玩这个游戏」→ 选 A/B;你想「玩这份代码」→ 走本文档。

---

## 18. 每次收工前(为了明天换机不用重做)

阶段之间的状态**必须能靠仓库本身恢复**,不要靠记忆:

```sh
cd /e/AndroidProject/TankTv
git status                 # 确认改了什么
git add -A && git commit -m "阶段 C: 游戏源码已编入,可启动(无资源)"
```

收工前 30 秒检查清单:

- [ ] 本文档第 P 节的**进度看板**已更新(状态/下一步)
- [ ] 本文档第 15 节的「侦察结果」已填
- [ ] 当前卡住的报错**原文**已贴进 git commit 信息或临时文件(别只写“编译失败”)
- [ ] `third_party/` 里的库版本写清楚了(SDL2 = 2.32.4 等),因为它不进版本库、换机要重拉
- [ ] 如果改过 `CMakeLists.txt` 里的相对路径,把**最终生效的那一行**粘到本文档 2B.5 下面

新机器开工顺序:**第 2B 节(把坑重演一遍 + 恢复环境)→ 第 P 节看板 → 找到 ⬜ 的下一项**。

---

## 19. 换电脑后的「五分钟恢复」(专用)

### 19.1 如果只想要代码(新机器上从零开始)

```sh
# 注意:third_party/ 与 game/ 被 .gitignore 排除了,clone 下来只有 35 个文件(约 200KB)
git clone https://github.com/zccguagua/TankTv.git
cd TankTv

# 然后按 4.1 重新拉两个依赖:
#   third_party/SDL  → codeload zip(见 2B.2)
#   game/            → codeload zip(见 2B.2)
```

### 19.2 如果就在同一台机器上继续

```sh
# 1. 确认工具链(只要这三行都有输出就能开工)
java -version; adb version; ls /d/Sdk/ndk

# 2. 确认 Gradle 可用(下载见 2B.4)
ls /d/tools/gradle-8.1.1/bin/gradle

# 3. 确认工程完好
cd /e/AndroidProject/TankTv
ls game/src/main.cpp        # 游戏源码在
ls third_party/SDL/CMakeLists.txt   # SDL 在
ls app/app/jni/CMakeLists.txt       # 工程在

# 4. 直接重建一次,看是不是还是 SUCCESSFUL(增量构建约 20~30 秒)
cd app
/d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon --console=plain | tail -5
```

只要第 4 步是 `BUILD SUCCESSFUL`,就可以直接接阶段 C。

> 如果换的是**全新的机器**,依赖需要重新准备:
> 1. 装 JDK 17 + SDK(platform 34)+ NDK r26d + CMake 3.22.1
> 2. 重新拉 `third_party/SDL`(2B.2 的 codeload 方法)与 `game/`
> 3. 下 Gradle 镜像包(2B.4)
> 4. 改 `app/local.properties` 的 `sdk.dir` 和 `app/app/build.gradle` 的 `ndkPath`
> 5. 跑第 4 步验证
