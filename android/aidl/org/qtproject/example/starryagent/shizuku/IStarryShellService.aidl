package org.qtproject.example.starryagent.shizuku;

interface IStarryShellService {

    void destroy() = 16777114;

    String exec(String shell, String command, String workingDirectory) = 1;
}
