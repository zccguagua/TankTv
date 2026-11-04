# 装到电视后要确认 / 要发给我的东西

> 临时文件,阶段 B 验收完就可以删(或留作记录)。
> 目的:一次把该记录的信息收齐,别装完之后想不起来当时是什么现象。

---

## 0. 一句话背景

现在这个 APK 是**阶段 B 的引导程序**,不是游戏。
屏幕上应该只有**缓慢变色的蓝底**,没有坦克。这是正常的。

---

## 1. 装之前先确认

```sh
cd /e/AndroidProject/TankTv
adb connect 电视IP:5555
adb devices
```

- [ ] `adb devices` 里电视的状态是 `device`(不是 `offline` / `unauthorized`)
- [ ] 电视上已开「开发者选项 → USB 调试 / 网络调试」

> 电视 IP 在哪看:设置 → 网络 → 当前连接 → IP 地址。

---

## 2. 安装并启动

```sh
adb install -r app/app/build/outputs/apk/debug/app-debug.apk
adb shell cmd package resolve-activity --brief com.example.tanktv
```

- [ ] `install` 输出 `Success`
- [ ] 电视首页能找到这个 app(图标是个坦克绿的方块,banner 上写着 TANK TV)

> 如果首页没有:先把 tv_banner 做出来这次已经加了 LEANBACK_LAUNCHER,
> 理论上应该出现。若仍不出现,用这条直接启动:
> `adb shell am start -n com.example.tanktv/org.libsdl.app.SDLActivity`

---

## 3. 抓日志(最重要)

**先清空,再启动,再操作**,这样日志干净:

```sh
adb logcat -c
adb shell am start -n com.example.tanktv/org.libsdl.app.SDLActivity
adb logcat | grep -iE "SDL|TankTv|KEYDOWN|KEYUP|AndroidRuntime"
```

### 需要确认的第一组:程序有没有真正跑起来

- [ ] 看到 `=== TankTv bootstrap ===`
- [ ] 看到 `SDL version: 2.32.4`
- [ ] 看到 `renderer: OpenGL ES 2.0`  ← **这一行很关键**

### 把下面这几条的实际内容发我

```
=== TankTv bootstrap ===            ← 有 / 没有
SDL version: ______                 ← 期望 2.32.4
renderer: ______                    ← 期望 OpenGL ES 2.0(如果是 software 一定要告诉我)
```

---

## 4. 屏幕现象(肉眼确认)

- [ ] 蓝底在**缓慢变化颜色**(说明在逐帧刷新,不是卡死的一张图)
- [ ] 是**全屏**,没有黑边、没有被切掉一部分
- [ ] 没有任何弹窗 / 报错框

如果蓝底不动了、或者黑屏:

- 黑屏但日志正常 → 可能只是渲染没刷新,告诉我
- 直接崩回桌面 → 日志里会有 `AndroidRuntime: FATAL`,**把那段完整贴我**

---

## 5. 遥控器按键(阶段 F 要用的数据)

**在蓝底界面上,用遥控器依次按**:

1. 上
2. 下
3. 左
4. 右
5. OK / 确定 / 中间的圆键
6. 返回

每按一次,日志会打两行左右,形如:

```
KEYDOWN scancode=82 sym=1073741906
KEYUP   scancode=82 sym=1073741906
```

### 把下面这张表填好发我(照抄日志里的数字)

| 按了什么 | scancode | sym |
|---|---|---|
| 上 | | |
| 下 | | |
| 左 | | |
| 右 | | |
| **OK / 确定** | | |
| 返回 | | |

**为什么重要**:游戏里玩家1开火键是「右 Ctrl」(`KEY_RCTRL`)。遥控器的 OK 键肯定不是这个,
到时候要在代码里把遥控器 OK 映射成开火。没有这张表就得盲猜。

> 如果按了没反应(日志完全没新增),也告诉我 —— 说明 SDL 没收到按键事件,
> 那是另一个要解决的问题(可能要改 SDLActivity 的按键转发)。

---

## 6. 顺带确认(可选但有用)

- [ ] 电视的 Android 版本(`adb shell getprop ro.build.version.release`)
- [ ] CPU 架构(`adb shell getprop ro.product.cpu.abi`)→ 期望 `arm64-v8a`
- [ ] 品牌型号(`adb shell getprop ro.product.model`)

如果 `ro.product.cpu.abi` **不是** `arm64-v8a`(比如是 `armeabi-v7a`),
说明我编的包装不上 —— 立刻告诉我,我把 ABI 补上重编。

---

## 7. 一句话总结模板(直接复制这段填完发我)

```
装上了:是/否
首页能看到图标:是/否
日志三条:
  bootstrap 行:有/没有
  SDL version:
  renderer:
画面:蓝底在变色 / 黑屏 / 崩溃
崩溃的话日志:
按键表:
  上=/ 下=/ 左=/ 右=/ OK=/ 返回=
电视:Android 版本 / abi / 型号
额外问题(有就说):
```

---

## 附:卸载 / 重装

```sh
# 重装(覆盖)
adb install -r app/app/build/outputs/apk/debug/app-debug.apk

# 卸载
adb uninstall com.example.tanktv

# 断开电视
adb disconnect
```
