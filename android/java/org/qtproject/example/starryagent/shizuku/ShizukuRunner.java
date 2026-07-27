package org.qtproject.example.starryagent.shizuku;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.content.ComponentName;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

import rikka.shizuku.Shizuku;

public final class ShizukuRunner {

    private static final String TAG = "StarryShizuku";
    private static final String APP_ID = "org.qtproject.example.starryagent";
    private static final int REQUEST_CODE = 22330;
    private static final int SERVICE_VERSION = 1;

    private ShizukuRunner() {
    }

    public static String exec(String shell, String command, String workingDirectory) {
        try {
            if (!Shizuku.pingBinder()) {
                return "Error: Shizuku unavailable. Start Shizuku and retry.";
            }
            if (Shizuku.isPreV11()) {
                return "Error: Shizuku pre-v11 is unsupported.";
            }
            if (Shizuku.checkSelfPermission() != PERMISSION_GRANTED) {
                requestPermissionAsync();
                return "Error: Shizuku permission not granted. Permission request sent; retry after granting.";
            }

            CountDownLatch latch = new CountDownLatch(1);
            AtomicReference<String> resultRef = new AtomicReference<>();
            AtomicReference<String> errorRef = new AtomicReference<>();
            Shizuku.UserServiceArgs args = new Shizuku.UserServiceArgs(
                    new ComponentName(APP_ID, StarryShellService.class.getName()))
                    .daemon(false)
                    .tag("starry-shell")
                    .version(SERVICE_VERSION)
                    .debuggable(false)
                    .processNameSuffix("starry_shell");

            ServiceConnection connection = new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    try {
                        IStarryShellService shellService = IStarryShellService.Stub.asInterface(service);
                        resultRef.set(shellService.exec(shell, command, workingDirectory));
                    } catch (Throwable t) {
                        errorRef.set("Error: Shizuku service call failed: " + Log.getStackTraceString(t));
                    } finally {
                        try {
                            Shizuku.unbindUserService(args, this, true);
                        } catch (Throwable ignored) {
                        }
                        latch.countDown();
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    errorRef.set("Error: Shizuku service disconnected.");
                    latch.countDown();
                }
            };

            Shizuku.bindUserService(args, connection);
            if (!latch.await(30, TimeUnit.SECONDS)) {
                try {
                    Shizuku.unbindUserService(args, connection, true);
                } catch (Throwable ignored) {
                }
                return "Error: Timed out waiting for Shizuku service.";
            }
            if (errorRef.get() != null) {
                return errorRef.get();
            }
            return resultRef.get() != null ? resultRef.get() : "Error: Empty result from Shizuku service.";
        } catch (Throwable t) {
            return "Error: Shizuku exec bridge failed: " + Log.getStackTraceString(t);
        }
    }

    private static void requestPermissionAsync() {
        try {
            new Handler(Looper.getMainLooper()).post(() -> {
                try {
                    Shizuku.requestPermission(REQUEST_CODE);
                } catch (Throwable t) {
                    Log.e(TAG, "requestPermission", t);
                }
            });
        } catch (Throwable t) {
            Log.e(TAG, "requestPermissionAsync", t);
        }
    }
}
