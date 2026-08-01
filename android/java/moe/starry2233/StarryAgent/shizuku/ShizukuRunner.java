package moe.starry2233.StarryAgent.shizuku;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.content.ComponentName;
import android.content.ServiceConnection;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import rikka.shizuku.Shizuku;

public final class ShizukuRunner {

    private static final String TAG = "StarryShizuku";
    private static final String APP_ID = "moe.starry2233.StarryAgent";
    private static final int REQUEST_CODE = 22330;
    private static final int SERVICE_VERSION = 1;

    private ShizukuRunner() {
    }

    public static String exec(String shell, String command, String workingDirectory) {
        try {
            if (!Shizuku.pingBinder()) {
                return jsonResult(false, "shizuku_unavailable", "", "", -1,
                        "Start Shizuku and retry.");
            }
            if (Shizuku.isPreV11()) {
                return jsonResult(false, "service_error", "", "", -1,
                        "Shizuku pre-v11 is unsupported.");
            }
            if (Shizuku.checkSelfPermission() != PERMISSION_GRANTED) {
                requestPermissionAsync();
                return jsonResult(false, "permission_required", "", "", -1,
                        "Shizuku permission request sent; retry after granting.");
            }

            CountDownLatch latch = new CountDownLatch(1);
            AtomicReference<String> resultRef = new AtomicReference<>();
            AtomicReference<String> errorRef = new AtomicReference<>();
            AtomicBoolean finished = new AtomicBoolean(false);
            Shizuku.UserServiceArgs args = createUserServiceArgs();

            ServiceConnection connection = new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    try {
                        IStarryShellService shellService = IStarryShellService.Stub.asInterface(service);
                        String result = shellService.exec(shell, command, workingDirectory);
                        resultRef.set(result);
                    } catch (Throwable t) {
                        errorRef.set(jsonResult(false, "service_error", "", "", -1,
                                "Shizuku service call failed: " + Log.getStackTraceString(t)));
                    } finally {
                        tryUnbind(args, this);
                        if (finished.compareAndSet(false, true)) {
                            latch.countDown();
                        }
                    }
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    if (finished.compareAndSet(false, true)) {
                        errorRef.set(jsonResult(false, "service_disconnected", "", "", -1,
                                "Shizuku service disconnected."));
                        latch.countDown();
                    }
                }
            };

            try {
                Shizuku.bindUserService(args, connection);
            } catch (Throwable t) {
                return jsonResult(false, "bind_failed", "", "", -1,
                        "Failed to bind Shizuku user service: " + Log.getStackTraceString(t));
            }

            if (!latch.await(30, TimeUnit.SECONDS)) {
                tryUnbind(args, connection);
                return jsonResult(false, "service_timeout", "", "", -1,
                        "Timed out waiting for Shizuku service.");
            }
            if (errorRef.get() != null) {
                return errorRef.get();
            }
            if (resultRef.get() == null || resultRef.get().isEmpty()) {
                return jsonResult(false, "service_error", "", "", -1,
                        "Empty result from Shizuku service.");
            }
            return resultRef.get();
        } catch (Throwable t) {
            return jsonResult(false, "service_error", "", "", -1,
                    "Shizuku exec bridge failed: " + Log.getStackTraceString(t));
        }
    }

    private static Shizuku.UserServiceArgs createUserServiceArgs() {
        return new Shizuku.UserServiceArgs(
                new ComponentName(APP_ID, StarryShellService.class.getName()))
                .daemon(false)
                .tag("starry-shell")
                .version(SERVICE_VERSION)
                .debuggable(false)
                .processNameSuffix("starry_shell");
    }

    private static void tryUnbind(Shizuku.UserServiceArgs args, ServiceConnection connection) {
        try {
            Shizuku.unbindUserService(args, connection, true);
        } catch (Throwable ignored) {
        }
    }

    private static String jsonResult(boolean ok, String status, String stdout,
                                     String stderr, int exitCode, String message) {
        try {
            JSONObject object = new JSONObject();
            object.put("ok", ok);
            object.put("status", status);
            object.put("stdout", stdout == null ? "" : stdout);
            object.put("stderr", stderr == null ? "" : stderr);
            object.put("exitCode", exitCode);
            object.put("message", message == null ? "" : message);
            return object.toString();
        } catch (JSONException e) {
            Log.e(TAG, "jsonResult", e);
            return "{\"ok\":false,\"status\":\"service_error\",\"stdout\":\"\",\"stderr\":\"\",\"exitCode\":-1,\"message\":\"Failed to encode bridge result\"}";
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
