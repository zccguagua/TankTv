package com.zcc.tanktv;

import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * 阶段 D:把 APK 里的 assets 解包到私有目录,并切换进程工作目录。
 *
 * 游戏源码里所有资源路径都是相对路径(appconfig.h):
 *     texture.png / kongtext.ttf / stages/N / sounds/xxx.ogg
 * 上游代码不改,所以只能在启动前 chdir 到这个目录。
 *
 * 顺序很重要:资源要在游戏 main() 之前就绪。
 * SDL 的 native 线程要等首次 onResume + surface 就绪后才创建,
 * 所以先在 onCreate 里解包,再调 nativeChdir,时间上一定来得及。
 */
public class GameActivity extends SDLActivity {
    private static final String TAG = "TankTv";

    /** assets 内容变了就 +1,下次启动会自动重新解包 */
    private static final int ASSETS_VERSION = 1;
    private static final String ASSETS_DIR = "assets";
    private static final String MARKER = ".extracted_version";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // 解包只用到 Context,可以放在 super 之前(尽量别拖慢 SDL 的初始化)
        File dir = prepareAssets();

        // super.onCreate() 会加载 libmain.so(里面才有 nativeChdir)
        super.onCreate(savedInstanceState);

        // SDLMain 线程要等首次 onResume + surface 就绪才启动,
        // 所以在这里改工作目录一定早于游戏 main()
        if (!nativeChdir(dir.getAbsolutePath())) {
            // 失败的话游戏会因为找不到资源而崩溃,先在这里留下线索
            Log.e(TAG, "nativeChdir failed: " + dir.getAbsolutePath());
        }
    }

    private static native boolean nativeChdir(String path);

    /**
     * SDL 默认去 dlsym("SDL_main") 找入口,而游戏的 main.cpp 没有 include SDL.h,
     * 导出的符号就叫 main(已用 llvm-nm 确认是导出的全局符号)。
     * 这里直接告诉 SDL 用 "main",不用在上游代码里加任何东西。
     */
    @Override
    protected String getMainFunction() {
        return "main";
    }

    /**
     * 阶段 F:把遥控器上的「空闲键」统一映射成开火。
     *
     * 游戏玩家 1 的开火键写死是右 Ctrl(KeyCode::KEY_RCTRL),遥控器没有这个键。
     * 而遥控器的 OK 键 = 回车 = 游戏里的「暂停」,不能拿来开火。
     *
     * 之所以在 Java 层拦、而不是在 SDL 事件过滤器里拦:SDL 的 Android 键表会把
     * 频道±、彩色键这类键映射成 SDL_SCANCODE_UNKNOWN,然后在 SDL_SendKeyboardKey
     * 里直接丢掉(不发事件)。在这里用原始 Android keycode 拦截,才能覆盖到这些键。
     *
     * 映射成 KEYCODE_CTRL_RIGHT(Android 114)后,SDL 会把它译成 SDL_SCANCODE_RCTRL,
     * 正好就是游戏的开火键。
     */
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        int keyCode = event.getKeyCode();
        if (isFireSourceKey(keyCode)) {
            int action = event.getAction();
            if (action == KeyEvent.ACTION_DOWN) {
                if (event.getRepeatCount() == 0) {
                    Log.i(TAG, "fire key: " + keyCode + " -> KEYCODE_CTRL_RIGHT");
                }
                SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_CTRL_RIGHT);
                return true;
            } else if (action == KeyEvent.ACTION_UP) {
                SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_CTRL_RIGHT);
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    /**
     * 可当开火键的空闲键。覆盖常见 Android TV 遥控器的菜单/频道/彩色/媒体键,
     * 保证换一台没有「菜单」键的电视也能开火。
     */
    private static boolean isFireSourceKey(int keyCode) {
        switch (keyCode) {
            case KeyEvent.KEYCODE_MENU:                  // 82  菜单
            case KeyEvent.KEYCODE_CHANNEL_UP:            // 166 频道+
            case KeyEvent.KEYCODE_CHANNEL_DOWN:          // 167 频道-
            case KeyEvent.KEYCODE_PROG_RED:              // 183 红
            case KeyEvent.KEYCODE_PROG_GREEN:            // 184 绿
            case KeyEvent.KEYCODE_PROG_YELLOW:           // 185 黄
            case KeyEvent.KEYCODE_PROG_BLUE:             // 186 蓝
            case KeyEvent.KEYCODE_GUIDE:                 // 172 节目指南
            case KeyEvent.KEYCODE_INFO:                  // 165 信息
            case KeyEvent.KEYCODE_SETTINGS:              // 176 设置
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:      // 85  播放/暂停
            case KeyEvent.KEYCODE_MEDIA_STOP:            // 86  停止
            case KeyEvent.KEYCODE_BOOKMARK:              // 174 收藏
            case KeyEvent.KEYCODE_CAPTIONS:              // 175 字幕
            case KeyEvent.KEYCODE_DVR:                   // 173 录制
            case KeyEvent.KEYCODE_TV_CONTENTS_MENU:      // 256 内容菜单
            case KeyEvent.KEYCODE_TV_MEDIA_CONTEXT_MENU: // 257 媒体上下文菜单
                return true;
            default:
                return false;
        }
    }

    /** 返回解包后的资源目录(也是游戏的 CWD) */
    private File prepareAssets() {
        File dir = new File(getFilesDir(), ASSETS_DIR);
        File marker = new File(dir, MARKER);

        if (marker.isFile() && readInt(marker) == ASSETS_VERSION) {
            return dir;
        }

        Log.i(TAG, "extracting assets -> " + dir.getAbsolutePath());
        if (!dir.exists() && !dir.mkdirs()) {
            Log.e(TAG, "cannot create " + dir.getAbsolutePath());
            return dir;
        }

        try {
            copyAssetDir(getAssets(), "", dir);
            writeInt(marker, ASSETS_VERSION);
            Log.i(TAG, "extract done");
        } catch (IOException e) {
            // 不写 marker,下次启动会重试
            Log.e(TAG, "extract failed", e);
        }
        return dir;
    }

    private void copyAssetDir(AssetManager am, String assetPath, File destDir) throws IOException {
        String prefix = assetPath.isEmpty() ? "" : assetPath + "/";
        String[] names = am.list(assetPath);
        if (names == null || names.length == 0) {
            copyAssetFile(am, assetPath, new File(destDir, new File(assetPath).getName()));
            return;
        }
        if (!destDir.isDirectory() && !destDir.mkdirs()) {
            throw new IOException("mkdirs failed: " + destDir);
        }
        for (String name : names) {
            String childAsset = prefix + name;
            File childDest = new File(destDir, name);
            String[] children = am.list(childAsset);
            if (children != null && children.length > 0) {
                copyAssetDir(am, childAsset, childDest);
            } else {
                copyAssetFile(am, childAsset, childDest);
            }
        }
    }

    private void copyAssetFile(AssetManager am, String assetPath, File dest) throws IOException {
        File parent = dest.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("mkdirs failed: " + parent);
        }
        try (InputStream in = am.open(assetPath);
             OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
        }
    }

    private static int readInt(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            int b = in.read();
            return b < 0 ? -1 : b;
        } catch (IOException e) {
            return -1;
        }
    }

    private static void writeInt(File f, int value) throws IOException {
        try (OutputStream out = new FileOutputStream(f)) {
            out.write(value);
        }
    }
}
