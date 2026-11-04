# TankTv

把经典坦克大战(FC《Battle City》/ 90 坦克)跑在 Android 电视上的实验项目。

游戏本体用的是开源复刻 [krystiankaluzny/Tanks](https://github.com/krystiankaluzny/Tanks)(C++ + SDL2,MIT 许可),
本仓库负责把它包成能在 Android TV 上用遥控器玩的 APK。

> 本项目**不包含**游戏源码,也不包含 SDL2 源码 —— 两者都在 `.gitignore` 里,需要按下面步骤自行获取。

---

## 当前进度

| 阶段 | 内容 | 状态 |
|---|---|---|
| 0 | 环境(SDK/NDK/CMake) | ✅ 完成 |
| B | SDL2 模板跑出 APK | ✅ 完成(已能在电视显示渐变画面) |
| C | 接入游戏源码 | ⬜ 进行中 |
| D | 资源解包 | ⬜ |
| E | 声音 | ⬜ |
| F | 遥控器 / 触摸输入 | ⬜ |
| G | 文字渲染 | ⬜ |
| H–J | 存档 / 缩放 / TV 打包 | ⬜ |

详细步骤、每步的命令与验收标准、以及已经踩过的坑,全都记在
**[ANDROID_PORT_PLAN.md](ANDROID_PORT_PLAN.md)** 里。

装到电视后要记录什么,看 **[TMP_TV_CHECK.md](TMP_TV_CHECK.md)**。

---

## 目录结构

```
TankTv/
├── ANDROID_PORT_PLAN.md   施工文档(进度看板在最前面)
├── TMP_TV_CHECK.md        装机检查清单
├── README.md
├── app/                   Android 工程(拷自 SDL2 官方 android-project)
│   ├── app/               模块(注意双层同名)
│   │   ├── build.gradle   已改为 CMake + applicationId
│   │   ├── jni/           CMakeLists 指向 third_party/SDL
│   │   └── src/main/
│   └── local.properties   本机 SDK 路径(不进版本库)
├── game/                  ← 需自行下载,游戏源码
└── third_party/SDL/       ← 需自行下载,SDL 2.32.4
```

---

## 快速开始

### 1. 准备依赖

```sh
# SDL2
curl -L -o /tmp/SDL.zip \
  https://codeload.github.com/libsdl-org/SDL/zip/refs/tags/release-2.32.4
unzip -q /tmp/SDL.zip -d /tmp && mv /tmp/SDL-release-2.32.4 third_party/SDL

# 游戏源码
curl -L -o /tmp/Tanks.zip \
  https://codeload.github.com/krystiankaluzny/Tanks/zip/refs/heads/master
unzip -q /tmp/Tanks.zip -d /tmp && mv /tmp/Tanks-master game
```

> 用 `codeload` 而不是 `git clone`,是因为部分网络环境下 `github.com` 连不上,
> 但 `codeload.github.com` 可以。

### 2. 配置本机路径

`app/local.properties`:

```properties
sdk.dir=D\:\\Sdk
cmake.dir=D\:\\Sdk\\cmake\\3.22.1
```

NDK 路径在 `app/app/build.gradle` 的 `ndkPath`,或用 `-PndkPath=...` 覆盖。

### 3. 构建

```sh
cd app
gradle assembleDebug
```

产物:`app/app/build/outputs/apk/debug/app-debug.apk`

> 本机 Gradle 是从镜像装的(官方包会重定向到 GitHub),路径见施工文档 2B.4。

### 4. 装到电视

```sh
adb connect 电视IP:5555
adb install -r app/app/build/outputs/apk/debug/app-debug.apk
```

---

## 环境要求

| 组件 | 版本 |
|---|---|
| JDK | 17 |
| Android SDK | platform 34 |
| NDK | r26d(或 r27) |
| CMake | 3.22.1 |
| Gradle | 8.1.1 |
| 设备 | arm64-v8a 的 Android TV / 盒子 |

---

## 已知问题

- **Gradle 官方发行包下不下来**:`services.gradle.org` 重定向到 GitHub,网络受限时会失败。用腾讯云/华为云镜像。
- **Gradle 报 `Connection refused: 127.0.0.1:7890`**:全局 `gradle.properties` 里配了代理但代理没开。
- **电视首页看不到应用**:必须有 `CATEGORY_LEANBACK_LAUNCHER` 和 banner(已加)。

更完整的排查表见施工文档第 14 节。

---

## 许可

- 本仓库代码:未指定(个人项目)
- 游戏源码:[MIT](https://github.com/krystiankaluzny/Tanks/blob/master/LICENSE)
- SDL2:[zlib](https://github.com/libsdl-org/SDL/blob/main/LICENSE.txt)

游戏玩法本身来自 Namco 的《Battle City》,但本项目不包含任何原版 ROM 或素材。
