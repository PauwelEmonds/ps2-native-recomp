// GameActivity: SDL's activity running the game (libmain.so: host/android_main.cpp, then host/main.cpp as on the PC) with the
// command line ps2run gets on the PC: --disc <image> --window --game-dir <the app's folder> --memcard <folder>/<game>.ps2.
// The app's folder (Android/data/<app id>/files) then holds the memory card, log.txt, keys.ini, quick saves and reports.
// Extras for testing: "args" adds options (e.g. "--frames 3000"), "env" sets PS2_ variables (e.g. "PS2_SCALE=2 PS2_GPU_HW=1").
package ps2native.app;

import android.content.Intent;
import android.os.Bundle;
import android.system.Os;
import android.util.Log;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class GameActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    // The PS2_ variables are set before SDLActivity.onCreate loads libmain.so: some are read while the library starts (its static
    // objects, e.g. PS2_PRESENT_LOG and PS2_FRAME_LOG), and getArguments() only runs after that.
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Intent intent = getIntent();
        String env = intent != null ? intent.getStringExtra("env") : null;
        if (env != null) setEnv(env.trim().split("\\s+"), true);
        // the game's own switches (games/<name>/env.txt on the PC, baked in by build.gradle); an "env" extra wins
        if (!BuildConfig.GAME_ENV.isEmpty()) setEnv(BuildConfig.GAME_ENV.split("\n"), false);
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getArguments() {
        Intent intent = getIntent();
        String disc = intent != null ? intent.getStringExtra("disc") : null;
        if (disc == null) disc = LauncherActivity.findDisc(this);   // started directly (am start), not from the launcher
        File dir = getExternalFilesDir(null);
        String folder = dir != null ? dir.getAbsolutePath() : getFilesDir().getAbsolutePath();
        List<String> a = new ArrayList<>();
        if (disc != null) { a.add("--disc"); a.add(disc); }
        a.addAll(Arrays.asList("--window", "--game-dir", folder, "--memcard", folder + "/" + BuildConfig.GAME + ".ps2"));
        String extra = intent != null ? intent.getStringExtra("args") : null;
        if (extra != null && !extra.trim().isEmpty()) a.addAll(Arrays.asList(extra.trim().split("\\s+")));
        Log.i(LauncherActivity.TAG, "arguments: " + a);
        return a.toArray(new String[0]);
    }

    private static void setEnv(String[] pairs, boolean overwrite) {
        for (String pair : pairs) {
            int eq = pair.indexOf('=');
            if (eq <= 0) continue;
            try { Os.setenv(pair.substring(0, eq), pair.substring(eq + 1), overwrite); }
            catch (Exception e) { Log.w(LauncherActivity.TAG, "setenv " + pair + ": " + e); }
        }
    }
}
