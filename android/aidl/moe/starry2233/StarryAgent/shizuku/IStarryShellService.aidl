package moe.starry2233.StarryAgent.shizuku;

interface IStarryShellService {

    void destroy() = 16777114;

    String exec(String shell, String command, String workingDirectory) = 1;

    String destroyAndExec(String shell, String command, String workingDirectory) = 2;
}
