# 装到电视后要确认 / 要发给我的东西

> 当前对应版本:**0.2.0(versionCode 2)** —— 功能上已经完成,这一轮是**验收**。
> 目的:一次把该记录的信息收齐,别装完之后想不起来当时是什么现象。

---

## 0. 一句话现状

游戏已经能玩:**移动 / 开火 / 文字 / 地图 / 高分保存** 都在了。
这轮只需要确认 6 件事(见第 4 节),**不用再改代码**。

唯一已知问题:**冷启动画面会缩在左上角**(退出重进就正常),可玩,排在最后修。

---

## 1. 安装

```sh
cd /e/AndroidProject/TankTv
adb install -r app/app/build/outputs/apk/debug/app-debug.apk
```

- [ ] `install` 输出 `Success`

> **TCL 电视**:如果报 `INSTALL_FAILED_VERIFICATION_FAILURE`,是电视自带的校验服务,
> 用 U 盘拷进去、用电视频文件管理器安装即可。

装完先回 TV 桌面看一眼:

- [ ] 「坦克大战」的卡片是**坦克图标**(不是 SDL 默认图标)
- [ ] banner 上是 **TANKS** 字样的图 + 一行 `ANDROID TV`
- [ ] 应用被归到「游戏」分类里(manifest 里加了 `isGame`)

---

## 2. 关键:先「完全退出」再启动(测冷启动全屏)

这一步很重要,已知问题只会在**冷启动**出现:

```sh
adb shell am force-stop com.zcc.tanktv
adb logcat -c
adb shell am start -n com.zcc.tanktv/com.zcc.tanktv.GameActivity
adb logcat | grep -iE "TankTv|SDL|AndroidRuntime"
```

### 2.1 日志里要有的几行(标签 `TankTv`)

```
chdir -> /data/user/0/com.zcc.tanktv/files/assets
highscore loaded: 2000                       ← 有存档时才会出现
resize helper: window 464x416 -> display 1920x1080 -> window 1920x1080
```

- [ ] 有 `chdir ->`
- [ ] 有 `resize helper: window ___x___ -> display ___x___ -> window ___x___`

**把 `resize helper` 这一整行原样抄给我**(括号里的数字决定下一步怎么修全屏)。

---

## 3. 画面(肉眼)

冷启动后:

- [ ] 有 **TANKS** LOGO + `1 PLAYER / 2 PLAYERS / EXIT` 文字
- [ ] 文字**不是**方块、空白或乱码
- [ ] 画面是**居中放大,左右有黑边**
- [ ] 如果画面缩在**左上角一小块** → 记下来(这是已知问题,我要这行的日志来定位)

---

## 4. 六项验收

| # | 验什么 | 怎么做 | 期望 |
|---|---|---|---|
| 1 | **开火** | 进关卡,按遥控器的 `菜单`/`频道±`/彩色键 | 打炮(按住连发) |
| 2 | **暂停** | 关卡里按 `返回` | 出现 PAUSE 画面;按 OK 继续 |
| 3 | **文字** | 看菜单 | `1 PLAYER` / `2 PLAYERS` / `EXIT` 正常显示 |
| 4 | **声音** | 切菜单、进关、开炮 | 有音效(菜单"哔"声、开场音) |
| 5 | **高分保存** | 打到 **超过 2000 分** → 完全退出 → 重开 | 菜单顶部 `HI- xxxx` 是那个分数 |
| 6 | **全屏** | 见第 2、3 节 | 居中放大 |

> 第 1 项如果按了没反应,**告诉我你按的是哪个键**(菜单/频道/彩色),
> 我在 `GameActivity.isFireSourceKey()` 里加一行就行。

---

## 5. 顺手确认(可选)

```sh
adb shell getprop ro.product.cpu.abi        # 期望 armeabi-v7a 或 arm64-v8a
adb shell getprop ro.build.version.release  # Android 版本
```

---

## 6. 直接复制这段填完发我

```
版本:0.2.0 装上了:是/否
TCL 那种校验失败:是/否

首页:
  坦克图标:是/否
  banner 正常:是/否
  归到游戏分类:是/否

冷启动:
  resize 那行日志(原样抄):
  chdir 那行:有/没有

画面:
  菜单有 LOGO + 文字:是/否
  居中放大有黑边:是/否
  缩在左上角:是/否

验收:
  1 开火(按的哪个键):
  2 暂停:
  3 文字:
  4 声音:
  5 高分保存:
  6 全屏:

额外问题(有就说):
```

---

## 7. 需要的话,抓张截图

```sh
adb shell screencap -p /sdcard/tank.png
adb pull /sdcard/tank.png .
```

---

## 附:常用命令

```sh
# 重装(覆盖)
adb install -r app/app/build/outputs/apk/debug/app-debug.apk

# 完全退出(测冷启动必用)
adb shell am force-stop com.zcc.tanktv

# 只清应用数据(会重新解包资源,高分也会没了)
adb shell pm clear com.zcc.tanktv

# 卸载
adb uninstall com.zcc.tanktv

# 断开电视
adb disconnect
```
