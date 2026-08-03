package moe.starry2233.StarryAgent;

import org.qtproject.qt.android.bindings.QtActivity;

public class StarryActivity extends QtActivity {
    @Override
    public void finish() {
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAfterTransition() {
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAndRemoveTask() {
        backgroundInsteadOfFinishing();
    }

    @Override
    public void finishAffinity() {
        backgroundInsteadOfFinishing();
    }

    private void backgroundInsteadOfFinishing() {
        if (isChangingConfigurations()) {
            super.finish();
            return;
        }
        moveTaskToBack(true);
    }
}
