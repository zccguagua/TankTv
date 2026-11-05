# 《坦克大战》(krystiankaluzny/Tanks) 移植到 Android 施工文档

> 目标:把 C++/SDL2 的桌面版 Battle City 复刻跑在 Android 上(手机 / Android TV)。
> 用法:本文件按「阶段」组织,每阶段都有 **命令 → 验收标准 → 卡住时的退路**。
> 换电脑后从「阶段 P」的恢复检查开始,按 `[ ]` 勾选继续即可。

---

## P. 进度看板(每次开工先看这里)

> 📁 **代码仓库**:https://github.com/zccguagua/TankTv (分支 `main`)

最近更新:**2026-11-05**(阶段 D 完成:资源进 APK、启动解包、chdir、等比缩放已就绪,待上电视验证)

| 阶段 | 状态 | 产物 / 证据 |
|---|---|---|
| 0 环境 | ✅ 完成 | JDK 17.0.10;SDK `D:/Sdk`(platform 34/35/36);NDK `android-ndk-r26d`;CMake 3.22.1 |
| A 桌面基线 | ⏭️ **跳过** | 非必需,直接进 Android |
| B 模板 APK | ✅ **完成**(已上电视验证) | 渐变画面正常,`renderer: OpenGL ES 2.0`;电视是 `armeabi-v7a` |
| C 接入游戏源码 | ✅ **完成** | APK 2.8MB;`libmain.so` 715KB(arm64)/ 388KB(v7a);无 undefined symbol |
| D 资源解包 | ✅ **完成**(待上电视验证) | APK 3.5MB;62 个 assets;`GameActivity` 解包+`chdir`;修了 `SDL_main` 入口和缩放竞态 |
| E 声音 | ⬜ 未开始 | 库已编进去(SDL_mixer + stb_vorbis),资源已就位,只剩验证 |
| F 输入 | ⬜ **下一步** | 遥控器映射已知:OK=Enter(40),返回=Escape(41);开火键需补(见 8.2) |
| G 文字 | ⬜ 未开始 | 现在用 stub,文字不显示 |
| H/I/J 收尾 | ⬜ 未开始 | — |

**下一步要做的第一件事**:把 `app-debug.apk` 装到电视,按 `TMP_TV_CHECK.md` 逐项确认。

阶段 D 之后屏幕应该长这样(文字还是空的,正常):

1. 黑色背景中间有 **TANKS 字样的大 LOGO**(来自 `texture.png` 图集)→ 说明贴图加载成功
2. LOGO 下面一个**坦克光标**(上下按方向键会移动)→ 说明遥控器事件通了
3. 按 **OK** 进选关,再按 **OK** 从第 1 关开始
4. 进关卡后能看到地图/砖墙/坦克 → 说明关卡文件也读到了

> ⚠️ 现在**还不能开火**:游戏 P1 的开火键写死是右 Ctrl(`KEY_RCTRL`),
> 遥控器 OK 是 Enter。这件事排在阶段 F。

> ⚠️ **本机的三个环境坑(已经在下面 B.2 记录,换机前务必先看)**:
>
> 1. `github.com` 直连被封 → 拉源码改走 `codeload.github.com` 下载 zip(可用)
> 2. `D:\.gradle\gradle.properties` 里写死了 `127.0.0.1:7890` 代理,代理不开时 Gradle 全部失败 → 构建时需临时禁用
> 3. Gradle 官方发行包会被重定向到 github.com(被封)→ 用腾讯云镜像下载,并用 `D:/tools/gradle-8.1.1/bin/gradle` 直接构建(绕开 wrapper)
>
> ⚠️ **`ndkPath` 是机器相关的**:`app/app/build.gradle` 里默认值已被改成
> 另一台电脑的 `C:/Users/Administrator/...`。在本机构建时请覆盖:
> ```sh
> /d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon \
>   -PndkPath=D:/Sdk/ndk/android-ndk-r26d
> ```

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
│   ├── SDL_image/              ← 2.8.8(PNG 走 stb_image)
│   └── SDL_mixer/              ← 2.8.1(OGG 走 stb_vorbis)
└── game/                       ← krystiankaluzny/Tanks 克隆(含 src/ 和 resources/)
```

> `app/app/jni/` 下有两个**我们自己的**辅助目录(会进版本库):
> - `include/SDL2/SDL_{image,mixer}.h` —— 转发头,把 `<SDL2/xxx.h>` 转到 `<xxx.h>`,
>   因为 libsdl 源码树的头文件不在 `SDL2/` 子目录下
> - `sdl_ttf_stub/` —— SDL_ttf 占位实现(头 + 5 个空函数),阶段 G 替换

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

**首次构建(旧机 `/e/AndroidProject/TankTv`,2026-11-04)**:

```
BUILD SUCCESSFUL
app/app/build/outputs/apk/debug/app-debug.apk   1.2 MB

APK 内容:
  lib/arm64-v8a/libSDL2.so       2.0 MB
  lib/arm64-v8a/libmain.so       6.6 KB
  lib/arm64-v8a/libc++_shared.so 1.3 MB
```

**实机验证(本机 `/f/AndroidProjects/TankTv`,2026-11-05)**:✅ 已装电视跑通。

换机器后本机环境与旧机不同,补齐这几样才装上电视:

- 本机 SDK = `C:\Users\Administrator\AppData\Local\Android\Sdk`(旧机是 `D:\Sdk`)
- 补装 NDK r26d → `Sdk\ndk\android-ndk-r26d`;CMake 3.22.1 → `Sdk\cmake\3.22.1`
- 重拉 SDL 源码 → `third_party/SDL`(codeload zip,见 2B.2)
- `app/app/build.gradle`:`ndkPath` 改成新路径;`abiFilters` 补上 `armeabi-v7a`(电视是 32 位)

电视验收结果:

- `ro.product.cpu.abi` = `armeabi-v7a`,安装成功,首页能看到图标(LEANBACK_LAUNCHER 生效)
- 画面:蓝底缓慢变红、平滑逐帧刷新、全屏、无黑边无弹窗
- 日志三条:
  - `=== TankTv bootstrap ===` ✅
  - `SDL version: 2.32.4` ✅
  - `renderer: opengles2` ✅ = OpenGL ES 2.0 硬件加速(非 software)

**遥控器按键表(阶段 F 直接抄,别再测)**:

| 按键 | scancode | sym | SDL 含义 |
|---|---|---|---|
| 上 | 82 | 1073741906 | SDL_SCANCODE_UP |
| 下 | 81 | 1073741905 | SDL_SCANCODE_DOWN |
| 左 | 80 | 1073741904 | SDL_SCANCODE_LEFT |
| 右 | 79 | 1073741903 | SDL_SCANCODE_RIGHT |
| OK / 确定 | 40 | 13 | SDL_SCANCODE_RETURN → 阶段 F 映射成开火 |
| 返回 | 41 | 27 | SDL_SCANCODE_ESCAPE → 阶段 F 改成暂停/返回 |

> 注意:OK 键 = 回车(不是玩家1开火键);返回键 = Escape。当前引导程序里
> `Escape` 会退出 app,所以按返回会直接退出——这是阶段 B 的预期行为,阶段 F 要去掉。

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
- `applicationId`(本项目的最终值是 `com.zcc.tanktv`,写在 `defaultConfig` 里)
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
| `defaultConfig.applicationId` | `"com.zcc.tanktv"` | **必须写在 defaultConfig 里**;可随时改,改完是另一个 app |
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
adb shell cmd package resolve-activity --brief com.zcc.tanktv
adb shell am start -n com.zcc.tanktv/org.libsdl.app.SDLActivity
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

### 5.4 ✅ 阶段 C 实施记录(2026-11-05 已完成)

实际做了什么:

1. **删掉** `app/app/jni/src/main.c`(引导程序,会与游戏的 `main` 重复定义)
2. `app/app/jni/src/CMakeLists.txt` 改为:
   ```cmake
   set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../game")
   file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
   message(STATUS "游戏源文件数量: ${GAME_SOURCE_COUNT}")   # 实测 34
   target_sources(main PRIVATE ${GAME_SOURCES} ...)
   target_include_directories(main PRIVATE "${GAME_DIR}/src" ...)
   target_link_libraries(main SDL2 SDL2_image SDL2_mixer)
   set_target_properties(main PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
   target_compile_options(main PRIVATE -Wno-narrowing)
   ```
3. `app/app/jni/CMakeLists.txt` 增加两个 `add_subdirectory`(SDL_image / SDL_mixer)
4. 新增 **两个辅助目录**(都在 `app/app/jni/` 下,会进版本库):
   - `sdl_ttf_stub/` —— SDL_ttf 占位实现(头 + 5 个函数)
   - `include/SDL2/SDL_{image,mixer}.h` —— 转发头

#### 关键发现 1:PNG 和 OGG 都不需要外部依赖

| 库 | 选项 | 效果 |
|---|---|---|
| SDL_image 2.8.8 | `SDL2IMAGE_BACKEND_STB` 默认 ON | PNG/JPEG 用 **stb_image**,不用编 libpng |
| SDL_mixer 2.8.1 | `SDL2MIXER_VORBIS` 默认 `STB` | OGG 用 **stb_vorbis**,不用编 libogg/libvorbis |

源码树里这两个单头文件都自带:`SDL_image/src/stb_image.h`、`SDL_mixer/src/codecs/stb_vorbis/stb_vorbis.h`。

#### 关键发现 2:`Mix_LoadWAV` 能加载 .ogg(但不能省 OGG 后端)

代码里音效全是 `.ogg` 却调 `Mix_LoadWAV()`,看起来像 bug,实际不是:
`Mix_LoadWAV_RW()` 读完前 4 字节后,非 `RIFF/WAVE` 会 fallback 到 `Mix_LoadMusic_RW()`
(`SDL_mixer/src/mixer.c:836`)。所以 **OGG 后端必须编进去**,否则返回 NULL。

#### 踩到的两个编译错误

| 报错 | 原因 | 修法 |
|---|---|---|
| `No cmake project for ogg found in external/ogg` | `SDL2MIXER_VENDORED` 默认开,要找 external/libogg | 传 `-DSDL2MIXER_VENDORED=OFF` 并关掉不用的编解码器(见 build.gradle)|
| `'SDL2/SDL_mixer.h' file not found` | 库里头文件是 `include/SDL_mixer.h`,游戏 include 带 `SDL2/` 前缀 | 加一层转发头 + `-I{here}/../include` |

#### 构建结果

```
BUILD SUCCESSFUL in 2m 25s
app-debug.apk                     2.8 MB
  arm64-v8a/libmain.so            715 KB   ← 游戏代码
  arm64-v8a/libSDL2.so            2.0 MB
  arm64-v8a/libSDL2_image.so      288 KB
  arm64-v8a/libSDL2_mixer.so      219 KB
  armeabi-v7a/libmain.so          388 KB   ← 你的电视用这个
  armeabi-v7a/libSDL2*.so         …
```

**无 undefined reference**,说明 IMG_*/TTF_*/Mix_* 都链上了。

#### 【重要】现在装上会崩,原因是缺资源

`engine/interactive_component.h:22` 在菜单项构造时就 `preload(sound)`:

```
Mix_LoadWAV("sounds/menu_item_selected.ogg") → 文件不存在 → NULL
  → sdl_sound_manager.cpp:124 throw Error("Failed to load sound: ...")
  → 未捕获异常 → 启动即退出
```

**所以阶段 C 只能验证「编得过」,要看到画面必须先完成阶段 D(资源)。**

### 5.5 快速定位「资源缺失」类崩溃

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

新建 `app/app/src/main/java/com/zcc/tanktv/GameActivity.java`(包名与 `applicationId` 一致):

```java
package com.zcc.tanktv;

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
<activity android:name="com.zcc.tanktv.GameActivity" ... >
```

> ⚠️ **必须写全限定名**,不能写 `.GameActivity`。
> 因为 `app/app/build.gradle` 里的 `namespace "org.libsdl.app"` 没改(SDL 的 Java 代码靠它),
> 写 `.GameActivity` 会被解析成 `org.libsdl.app.GameActivity` 而找不到类。

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

### 6.5 ✅ 阶段 D 实施记录(2026-11-05 已完成构建,待上电视)

> ⚠️ **实际实现和上面 6.1~6.3 的草案不一样**,以本节为准。
> 差别只有两处:解包目标是 `filesDir/assets`(多一层目录),`chdir` 不在游戏代码里改。

#### 6.5.1 实际做了什么(文件清单)

| 文件 | 动作 |
|---|---|
| `app/app/src/main/assets/` | **新增**,62 个文件共 708KB。**已进版本库**,换电脑不用重拷 |
| `app/app/src/main/java/com/zcc/tanktv/GameActivity.java` | **新增**,解包 + chdir + 覆盖 `getMainFunction()` |
| `app/app/jni/src/android_glue.c` | **新增**,JNI `chdir` + 补发一次 RESIZED 事件 |
| `app/app/jni/src/CMakeLists.txt` | 加 `android_glue.c` 到 `target_sources`;`target_link_libraries` 加 `log` |
| `app/app/src/main/AndroidManifest.xml` | `android:name="SDLActivity"` → `"com.zcc.tanktv.GameActivity"`(全限定名) |

资源进版本库的好处:**换电脑只 `git pull` 就有资源**,不用再跑 6.1 那三条 `cp`。

#### 6.5.2 解包 + 切目录的顺序(照抄就能用)

```java
protected void onCreate(Bundle savedInstanceState) {
    File dir = prepareAssets();      // 1. 解到 filesDir/assets(只用到 Context,可以放 super 之前)
    super.onCreate(savedInstanceState);  // 2. 这一步才 System.loadLibrary("SDL2"/"main")
    nativeChdir(dir.getAbsolutePath());  // 3. 必须在这里调:nativeChdir 在 libmain.so 里
}
```

**为什么 `chdir` 可以放在 `onCreate` 里**:游戏 `main()` 跑在 SDL 自己的线程上,
而那个线程要到**首次 `onResume` + surface 就绪 + 有焦点**时才创建
(`SDLActivity.handleNativeState()` 里 `new Thread(new SDLMain())`)。
`onCreate` 一定早于它。

**幂等**:解包目录里写 `.extracted_version`(内容是 `ASSETS_VERSION`),版本对上就跳过。
以后改 assets 就把 `ASSETS_VERSION` +1。

#### 6.5.3 【坑 1】`android.system.Os` 没有 `chdir`

```
错误: 找不到符号  方法 chdir(String)  位置: 类 Os
```

`android.system.Os` 里有一堆 `open/stat/mkdir/unlink…`,**就是没有 `chdir`**
(libcore 里那个 `Libcore.os.chdir` 是隐藏 API)。所以必须自己写 JNI:

```c
JNIEXPORT jboolean JNICALL
Java_com_zcc_tanktv_GameActivity_nativeChdir(JNIEnv *env, jclass clazz, jstring path)
{
    const char *c_path = (*env)->GetStringUTFChars(env, path, NULL);
    int rc = chdir(c_path);
    (*env)->ReleaseStringUTFChars(env, path, c_path);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}
```

JNI 里用了 `__android_log_print`,所以 **`target_link_libraries` 必须加 `log`**,
否则报 `undefined symbol: __android_log_print`。

#### 6.5.4 【坑 2,致命】SDL 找的是 `SDL_main`,上游只导出 `main`

这一条如果没修,**装上去就是黑屏/闪退**,而且日志里只有一句
`SDL_main not found` 之类的话,很容易误判成别的问题。

- SDL 的 Java 侧:`SDLActivity.getMainFunction()` 默认返回 `"SDL_main"`
- native 侧:`SDL_android.c` 里 `dlsym(libmain.so, "SDL_main")`
- 上游 `game/src/main.cpp` **没有** `#include <SDL.h>`,所以 `SDL_main.h` 里的
  `#define main SDL_main` 不生效 → 导出的符号名叫 **`main`**

实测确认:

```sh
# 只有 main,没有 SDL_main
llvm-nm -D --defined-only .../libmain.so | grep -E "main$"
0004e9f4 T main
```

**修法(不改上游代码)**:在 `GameActivity` 里覆盖一行:

```java
@Override
protected String getMainFunction() {
    return "main";
}
```

> 备选方案(没采用):加一个 `extern "C" int SDL_main(...)` 的包装文件,
> 但那样等于把 `main.cpp` 的内容复制一份,上游一改就要跟着改。
> 覆盖 `getMainFunction()` 更少、更稳。

#### 6.5.5 【坑 3,影响观感】缩放靠一条「可能拿不到」的事件

游戏靠 `SDLRenderer::setViewportForWindowSize()` 做等比缩放 + 居中,
而它**只在**收到 WINDOW 的 `RESIZED/MAXIMIZED/RESTORED/SHOWN` 时才被调用。

- `SHOWN` 事件带的是 `data1 = data2 = 0`(`SDL_video.c:2549` 就是这么发的)
  → 游戏里 `width <= 0` 直接 return,等于没用
- 真正有用的是 `RESIZED`,它来自 Java `surfaceChanged` → `onNativeResize`
- 但 `onNativeResize` 里有一句 `if (Android_Window)`,
  而那一刻游戏侧窗口经常**还没创建** → 事件被丢掉,之后也不会补

结果:渲染 scale 永远是 1,画面按 464×416 原尺寸画在**左上角**。

**修法**:`android_glue.c` 里起一个 pthread,等 `SDL_WasInit(SDL_INIT_VIDEO)` 为真、
`SDL_GetWindowFromID(1)` 拿到窗口后,把**真实的 surface 尺寸**塞一个 `RESIZED` 事件进队列:

```c
event.type = SDL_WINDOWEVENT;
event.window.event = SDL_WINDOWEVENT_RESIZED;
event.window.data1 = w;   // SDL_GetWindowSize 拿到的就是真实尺寸
SDL_PushEvent(&event);
```

游戏自己 poll 到就会算好缩放,上游代码零改动。

> 顺带搞清楚了一件事:`SDL_RenderSetViewport()` 传进去的 rect 会被 **乘以 scale**
> (`SDL_render.c:2487`),也就是 viewport 用的是**逻辑坐标**。
> 所以游戏里 `viewport.x = (width / scale - base.w) / 2` 的算法是对的,
> 464×416 在 1920×1080 上会算出 scale≈2.596、左右各留 ≈358px,是标准 letterbox。

**调试用日志**(logcat 里 grep `TankTv`):

```
chdir -> /data/user/0/com.zcc.tanktv/files/assets
resize helper: queued 1920x1080     ← 有这行说明 SDL 起来了、窗口也建了
```

#### 6.5.6 构建结果

```
BUILD SUCCESSFUL in 17s
app-debug.apk                     3.5 MB
assets/                          62 个文件(texture.png / kongtext.ttf / stages/0..35 / sounds/*.ogg)
arm64-v8a/libmain.so             716 KB
armeabi-v7a/libmain.so           388 KB     ← 你的电视用这个
```

导出的 JNI 与入口符号(实测):

```
T Java_com_zcc_tanktv_GameActivity_nativeChdir
T main
```

#### 6.5.7 装上后应该看到什么

| 画面 | 说明 |
|---|---|
| 黑底 + 中间 **TANKS** 大字 LOGO | `texture.png` 加载成功 |
| LOGO 下一个**坦克光标**,上下键能移动 | 遥控器事件通了(菜单项文字是空的,阶段 G 才有) |
| 按 **OK** → 灰底选关页(数字是空的),再按 **OK** | 进入第 1 关(`m_selected_stage` 默认 1) |
| 关卡里有砖墙、老鹰、坦克 | `stages/1` 读到了 |
| 坦克能动,**但不能开火** | 开火键是右 Ctrl,阶段 F 才映射遥控器 OK |

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
| `undefined symbol: __android_log_print` | `android_glue.c` 用了 log,但没 `target_link_libraries(... log)` | 加 `log`(见 6.5.3) |
| `找不到符号: 方法 chdir(String) 位置: 类 Os` | `android.system.Os` 没有 `chdir` | 用 JNI,见 6.5.3 |
| 启动黑屏/退出,日志提到 `SDL_main` | 上游只导出 `main` | 覆盖 `getMainFunction()` 返回 `"main"`,见 6.5.4 |
| 画面是左上角一小块 | 没收到 RESIZED 事件,scale 一直是 1 | 补发事件,见 6.5.5 |
| 启动即闪退,`AndroidRuntime: FATAL` | Java 侧异常 | `adb logcat -s AndroidRuntime` 看首行异常 |
| 黑屏但进程活着 | `SDL_main` 没接对 / 没进主循环 | 见 6.5.4;再在 main 里 `SDL_Log("alive")` 打点 |
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
[✓] IMG_Load 调用处数量:              3 处,全在 sdl_renderer.cpp
                                      IMG_Init(IMG_INIT_PNG) / IMG_LoadTexture / IMG_Quit
[✓] TTF_ 调用处:                      5 处,全在 sdl_renderer.cpp
                                      TTF_Init / TTF_OpenFont / TTF_RenderText_Solid
                                      TTF_CloseFont / TTF_Quit
                                      → 全部有判空,所以 stub 不会崩(只是没文字)
[✓] Mix_ 调用处:                      9 个函数,全在 sdl_sound_manager.cpp
                                      OpenAudio/CloseAudio/ChannelFinished/FreeChunk/
                                      HaltChannel/LoadWAV/PlayChannel/VolumeChunk/GetError
                                      ⚠️ Mix_LoadWAV 返回 NULL 会 throw(第 124 行)
[✓] 音效格式:                         .ogg(Vorbis),但用 Mix_LoadWAV 加载
                                      mixer.c:836 非 WAV 会 fallback 到 Mix_LoadMusic_RW
                                      → OGG 后端必须开
[ ] 写文件(高分)的方式:             待查(有 high_score.h / scores.cpp)
[✓] 窗口逻辑分辨率:                   464 x 416
                                      map 26x26 tiles x 16px = 416x416,status 3x16 = 48px
[✓] 玩家键位(appconfig.h):          P1 = ↑↓←→ + 右Ctrl(开火)
                                      P2 = W S A D + 左Ctrl
                                      ⚠️ 没有独立暂停键
[✓] 游戏完整度:                       stages_count = 35,每关 20 敌人(完整版)
[✓] APK 包名:                         com.zcc.tanktv(旧: com.example.tanktv)
[✓] Java 内 SDL 库名列表:            "SDL2", "main"(模板默认)
[✓] 遥控器方向键/OK 的 scancode:      见 2B.8 的按键表(已实测:OK=Enter 40、返回=Escape 41)
[✓] 上游 main() 导出的符号:           只有 `main`,**没有** `SDL_main`
                                      (main.cpp 没 include SDL.h)
                                      → 必须覆盖 GameActivity.getMainFunction()
[✓] Android 上缩放靠的事件:            RESIZED,来自 Java surfaceChanged->onNativeResize,
                                      和游戏建窗有竞态 → 需要在 native 侧补发一次(见 6.5.5)
[ ] Makefile 实际链接库:             SDL2main SDL2 SDL2_image SDL2_ttf SDL2_mixer
```

> **对阶段 D 的影响(重要)**:解包后的目录结构应该是
> ```
> <internal>/assets/        ← 实际实现是 filesDir/assets,chdir 到这里
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
# 但 app/app/src/main/assets/(62 个资源文件)是进版本库的,不用重拷
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
#    ⚠️ 必须带 -PndkPath,否则会用 build.gradle 里另一台机器的默认路径
cd app
/d/tools/gradle-8.1.1/bin/gradle assembleDebug --no-daemon --console=plain \
  -PndkPath=D:/Sdk/ndk/android-ndk-r26d | tail -5
```

只要第 4 步是 `BUILD SUCCESSFUL`,就可以接**阶段 F(遥控器开火键)**。

> 代理坑:`D:/.gradle/gradle.properties` 里写死的 `127.0.0.1:7890` 代理没开时,
> Gradle 会报 `Connection refused`。要么启动代理工具,要么临时把它挪走:
> ```sh
> cp /d/.gradle/gradle.properties /d/.gradle/gradle.properties.bak
> : > /d/.gradle/gradle.properties
> # …构建…
> cp /d/.gradle/gradle.properties.bak /d/.gradle/gradle.properties
> ```

> 如果换的是**全新的机器**,依赖需要重新准备:
> 1. 装 JDK 17 + SDK(platform 34)+ NDK r26d + CMake 3.22.1
> 2. 重新拉 `third_party/SDL`(2B.2 的 codeload 方法)与 `game/`
> 3. 下 Gradle 镜像包(2B.4)
> 4. 改 `app/local.properties` 的 `sdk.dir` 和 `app/app/build.gradle` 的 `ndkPath`
> 5. 跑第 4 步验证
