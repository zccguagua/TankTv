# 装到电视后要确认 / 要发给我的东西(阶段 D 版)

> 这份文件每次验收前更新。当前对应:**阶段 D(资源解包)**。
> 目的:一次把该记录的信息收齐,别装完之后想不起来当时是什么现象。

---

## 0. 一句话背景

现在这一版**已经是真正的游戏**了(资源已经打进 APK)。

但还有两件事没做,属于**已知情况,不是 bug**:

1. **文字全是空的**(菜单里的 `1 Player` / `2 Players` / `Exit` 不显示)—— 阶段 G
2. **不能开火**(能开坦克、能移动,但按 OK 不放炮)—— 阶段 F

声音应该已经有了(阶段 E 顺带验证)。

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
```

- [ ] `install` 输出 `Success`
- [ ] 电视首页能找到这个 app(名字「坦克大战」,banner 是 TANK TV)

> 如果首页没有,直接用这条启动(注意 activity 名变了):
> `adb shell am start -n com.zcc.tanktv/com.zcc.tanktv.GameActivity`

---

## 3. 抓日志(最重要)

**先清空,再启动**,这样日志干净:

```sh
adb logcat -c
adb shell am start -n com.zcc.tanktv/com.zcc.tanktv.GameActivity
adb logcat | grep -iE "TankTv|SDL|AndroidRuntime"
```

### 3.1 必须看到的两行(有 `TankTv` 标签)

```
chdir -> /data/user/0/com.zcc.tanktv/files/assets
resize helper: queued 1920x1080        ← 数字应该是你电视的分辨率
```

- [ ] 有 `chdir ->`
- [ ] 有 `resize helper: queued ____x____`

这两行**任何一行没有**都要告诉我,它们分别代表:

- 没有 `chdir` → 解包/切目录失败,资源读不到
- 没有 `resize` → SDL 视频没起来,或者窗口没建成(画面会是小窗或黑屏)

### 3.2 崩溃的话

```
AndroidRuntime: FATAL EXCEPTION ...
```
**把 `FATAL EXCEPTION` 到后面 20 行完整贴给我**(或者把整个 logcat 存文件发我)。

### 3.3 顺便记一下第一行解包耗时

第一次启动要解 62 个文件(708KB),应该 1 秒内完成。日志里会有:

```
extracting assets -> /data/user/0/com.zcc.tanktv/files/assets
extract done
```

之后每次启动会跳过(有 `.extracted_version` 标记)。

---

## 4. 屏幕现象(肉眼确认)

### 4.1 主菜单

- [ ] 黑底
- [ ] 中间偏上有 **TANKS** 大字 LOGO(不是模糊的方块)
- [ ] LOGO 下面有个**坦克图标**,按上/下方向键会换位置(有 3 个位置循环)
- [ ] 没有文字(正常,阶段 G)

### 4.2 进关卡

按 **OK**(遥控器中间的确认键):

- [ ] 屏幕变成**灰色背景**(这是选关页)
- [ ] 什么都看不到,只有灰底(正常:数字是文字,还没做)
- [ ] 再按一次 **OK** → 进入第 1 关

> 选关页默认就是第 1 关,直接按 OK 即可。左右键可以改关卡号(但看不到数字)。

### 4.3 关卡里

- [ ] 能看到**砖墙 / 钢墙 / 老鹰基地**
- [ ] 能看到自己的坦克(底部中间偏左)
- [ ] 敌方坦克会从上方出来
- [ ] 方向键能开自己的坦克
- [ ] **按 OK 不开火**(正常,阶段 F)

### 4.4 缩放是否正确(重点看这个)

- [ ] 游戏画面是**居中的、等比放大的**,左右有黑边(letterbox)
- [ ] **不是**挤在左上角的小方块

如果看到的是左上角一小块(464×416 原始尺寸),告诉我 ——
说明补发 RESIZED 事件那一步没生效,我再改。

---

## 5. 声音

- [ ] 进菜单/切菜单项有「哔」声
- [ ] 进关卡有开场音效
- [ ] 开炮应该有声音(但你现在没法开火,这条先跳过)

完全没声音也先告诉我 —— 可能是 `Mix_OpenAudio` 在电视上没起来,
下一轮我用 logcat 查。

---

## 6. 需要发我的内容(直接复制这段填)

```
装上了:是/否
首页能看到图标:是/否

日志:
  chdir 行:有/没有,内容是:
  resize 行:有/没有,内容是:
  extract done:有/没有
崩溃(有就贴 FATAL 那一段):

画面:
  主菜单有 TANKS LOGO:是/否
  坦克光标能上下移动:是/否
  能进第 1 关:是/否
  能看到砖墙/坦克:是/否
  坦克能用方向键开:是/否
  画面是居中放大(有黑边):是/否
  画面是左上角小方块:是/否

声音:有/无

电视:Android 版本 / abi / 型号
  (adb shell getprop ro.build.version.release)
  (adb shell getprop ro.product.cpu.abi)
  (adb shell getprop ro.product.model)

额外问题(有就说):
```

---

## 7. 如果画面位置不对,顺手抓一张截图

```sh
adb shell screencap -p /sdcard/tank.png
adb pull /sdcard/tank.png .
```

把 `tank.png` 发我,比文字描述快得多。

---

## 附:常用命令

```sh
# 重装(覆盖)
adb install -r app/app/build/outputs/apk/debug/app-debug.apk

# 卸载(想完全重来一次解包时用)
adb uninstall com.zcc.tanktv

# 只清应用数据(会重新走一遍解包)
adb shell pm clear com.zcc.tanktv

# 断开电视
adb disconnect
```
