package org.qtproject.example.starryagent.shizuku;

import android.content.Context;
import android.util.Log;

import androidx.annotation.Keep;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

public class StarryShellService extends IStarryShellService.Stub {

    private static final String TAG = "StarryShellService";

    public StarryShellService() {
        Log.i(TAG, "constructor");
    }

    @Keep
    public StarryShellService(Context context) {
        Log.i(TAG, "constructor with context=" + context);
    }

    @Override
    public void destroy() {
        Log.i(TAG, "destroy");
        System.exit(0);
    }

    @Override
    public String exec(String shell, String command, String workingDirectory) {
        if (!"sh".equals(shell)) {
            return "Error: Android shell_exec only supports `sh` through Shizuku.";
        }
        if (command == null || command.isEmpty()) {
            return "Error: `command` is required";
        }

        ProcessBuilder builder = new ProcessBuilder("sh", "-lc", command);
        if (workingDirectory != null && !workingDirectory.isEmpty()) {
            builder.directory(new java.io.File(workingDirectory));
        }

        try {
            Process process = builder.start();
            String stdout = readFully(process.getInputStream());
            String stderr = readFully(process.getErrorStream());
            int exitCode = process.waitFor();

            StringBuilder result = new StringBuilder();
            if (!stdout.isEmpty()) {
                result.append(stdout);
            }
            if (!stderr.isEmpty()) {
                if (result.length() > 0) {
                    result.append('\n');
                }
                result.append("[stderr] ").append(stderr);
            }
            result.append("\n[exit ").append(exitCode).append(']');
            return result.toString();
        } catch (Throwable t) {
            return "Error: Shizuku exec failed: " + Log.getStackTraceString(t);
        }
    }

    private static String readFully(java.io.InputStream stream) throws IOException {
        BufferedReader reader = new BufferedReader(new InputStreamReader(stream));
        StringBuilder builder = new StringBuilder();
        String line;
        boolean first = true;
        while ((line = reader.readLine()) != null) {
            if (!first) {
                builder.append('\n');
            }
            builder.append(line);
            first = false;
        }
        return builder.toString();
    }
}
