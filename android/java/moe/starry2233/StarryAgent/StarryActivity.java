package moe.starry2233.StarryAgent;

import android.Manifest;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Window;
import android.view.WindowManager;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;

import org.qtproject.qt.android.bindings.QtActivity;

public class StarryActivity extends QtActivity {
    private static final String TAG = "StarryActivity";
    private OnBackInvokedCallback backCallback;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        registerBackCallback();
        requestNotificationPermissionIfNeeded();
    }

    @Override
    public void onResume() {
        super.onResume();
        registerBackCallback();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus)
            interceptWsaCloseButton();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK &&
                event.getAction() == KeyEvent.ACTION_UP) {
            Log.i(TAG, "dispatchKeyEvent BACK");
            minimizeToBackground();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        Log.i(TAG, "onBackPressed");
        minimizeToBackground();
    }

    public void moveTaskToBackFromQt() {
        Log.i(TAG, "moveTaskToBackFromQt");
        runOnUiThread(this::backgroundInsteadOfFinishing);
    }

    public void handleQtWindowClose() {
        Log.i(TAG, "handleQtWindowClose");
        runOnUiThread(this::backgroundInsteadOfFinishing);
    }

    @Override
    public void finish() {
        Log.i(TAG, "finish");
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAfterTransition() {
        Log.i(TAG, "finishAfterTransition");
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAndRemoveTask() {
        Log.i(TAG, "finishAndRemoveTask");
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAffinity() {
        Log.i(TAG, "finishAffinity");
        backgroundInsteadOfFinishing();
    }

    private void registerBackCallback() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU || backCallback != null)
            return;
        backCallback = () -> {
            Log.i(TAG, "OnBackInvokedCallback");
            minimizeToBackground();
        };
        getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_OVERLAY, backCallback);
    }

    private void requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
            return;
        if (checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED)
            return;
        requestPermissions(new String[]{Manifest.permission.POST_NOTIFICATIONS}, 2233);
    }

    private void backgroundInsteadOfFinishing() {
        if (isChangingConfigurations()) {
            Log.i(TAG, "finish for configuration change");
            super.finish();
            return;
        }
        startBackgroundRuntime();
    }

    private void minimizeToBackground() {
        Log.i(TAG, "start foreground service, show background notification and moveTaskToBack");
        startBackgroundRuntime();
        moveTaskToBack(true);
    }

    private void startBackgroundRuntime() {
        Log.i(TAG, "start foreground service and show background notification");
        startBackgroundService();
        showBackgroundNotification();
    }

    private void interceptWsaCloseButton() {
        Window window = getWindow();
        if (window == null)
            return;
        WindowManager.LayoutParams params = window.getAttributes();
        if (params == null)
            return;
        params.setTitle("StarryAgent");
        window.setAttributes(params);
    }

    private void sendHomeToBackground() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
    }

    private void startBackgroundService() {
        Intent intent = new Intent(this, StarryBackgroundService.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
            startForegroundService(intent);
        else
            startService(intent);
    }

    private void showBackgroundNotification() {
        NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
        if (manager == null)
            return;
        manager.notify(StarryBackgroundService.NOTIFICATION_ID,
                StarryBackgroundService.createNotification(this));
    }
}
