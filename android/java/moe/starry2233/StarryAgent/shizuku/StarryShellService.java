package moe.starry2233.StarryAgent.shizuku;

import android.content.Context;
import android.util.Log;

import androidx.annotation.Keep;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.json.JSONException;
import org.json.JSONObject;

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
        return runCommand(shell, command, workingDirectory);
    }

    @Override
    public String destroyAndExec(String shell, String command, String workingDirectory) {
        return runCommand(shell, command, workingDirectory);
    }

    private static String runCommand(String shell, String command, String workingDirectory) {
        if (!"sh".equals(shell)) {
            return jsonResult(false, "invalid_args", "", "", -1,
                    "Android shell_exec only supports `sh` through Shizuku.");
        }
        if (command == null || command.isEmpty()) {
            return jsonResult(false, "invalid_args", "", "", -1,
                    "`command` is required");
        }

        ProcessBuilder builder = new ProcessBuilder("sh", "-lc", command);
        if (workingDirectory != null && !workingDirectory.isEmpty()) {
            java.io.File directory = new java.io.File(workingDirectory);
            if (!directory.isDirectory()) {
                return jsonResult(false, "invalid_args", "", "", -1,
                        "Working directory does not exist: " + workingDirectory);
            }
            builder.directory(directory);
        }

        try {
            Process process = builder.start();
            StreamCollector stdout = new StreamCollector(process.getInputStream());
            StreamCollector stderr = new StreamCollector(process.getErrorStream());
            stdout.start();
            stderr.start();

            int exitCode = process.waitFor();
            stdout.join();
            stderr.join();
            return jsonResult(true, "ok", stdout.output(), stderr.output(), exitCode,
                    exitCode == 0 ? "" : "Command exited with status " + exitCode);
        } catch (Throwable t) {
            return jsonResult(false, "process_error", "", "", -1,
                    Log.getStackTraceString(t));
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
            return "{\"ok\":false,\"status\":\"service_error\",\"stdout\":\"\",\"stderr\":\"\",\"exitCode\":-1,\"message\":\"Failed to encode service result\"}";
        }
    }

    private static String readFully(InputStream stream) throws IOException {
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

    private static final class StreamCollector extends Thread {

        private final InputStream stream;
        private String output = "";
        private IOException error;

        private StreamCollector(InputStream stream) {
            this.stream = stream;
        }

        @Override
        public void run() {
            try {
                output = readFully(stream);
            } catch (IOException e) {
                error = e;
            }
        }

        private String output() throws IOException {
            if (error != null) {
                throw error;
            }
            return output;
        }
    }
}
