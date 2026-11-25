// LauncherActivity: finds the disc image and starts the game (GameActivity). The image is, in this order:
//   1. a .iso in the app's own folder, Android/data/<app id>/files (copied there with `adb push` or over USB from a PC),
//      <game>.iso first;
//   2. the image chosen with the system's file picker on an earlier start (the app keeps the permission to read it);
//   3. otherwise the picker opens now. The game reads the chosen file through /proc/self/fd/<n>.
package ps2native.app;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;

public class LauncherActivity extends Activity {
    static final String TAG = "ps2native";
    private static final int REQ_PICK = 1;
    private static final String PREFS = "ps2native";
    private static final String KEY_DISC = "disc_uri";

    private TextView mText;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        String disc = findDisc(this);
        if (disc != null) { startGame(disc); return; }
        showChooser(null);
    }

    /** a disc image the game can open without asking: a .iso in the app's folder, or the image picked before; null if none */
    static String findDisc(Context c) {
        File dir = c.getExternalFilesDir(null);
        if (dir != null) {
            File own = new File(dir, BuildConfig.GAME + ".iso");
            if (own.isFile()) return own.getAbsolutePath();
            File[] files = dir.listFiles();
            if (files != null)
                for (File f : files)
                    if (f.isFile() && f.getName().toLowerCase().endsWith(".iso")) return f.getAbsolutePath();
        }
        String uri = c.getSharedPreferences(PREFS, MODE_PRIVATE).getString(KEY_DISC, null);
        return uri != null ? openUri(c, Uri.parse(uri)) : null;
    }

    /** /proc/self/fd/<n> for a document the app may read, or null. The descriptor stays open for the game in this process. */
    private static String openUri(Context c, Uri uri) {
        try {
            ParcelFileDescriptor pfd = c.getContentResolver().openFileDescriptor(uri, "r");
            if (pfd == null) return null;
            return "/proc/self/fd/" + pfd.detachFd();
        } catch (Exception e) {
            Log.w(TAG, "cannot open " + uri + ": " + e);
            return null;
        }
    }

    private void showChooser(String problem) {
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        int pad = (int) (24 * getResources().getDisplayMetrics().density);
        box.setPadding(pad, pad, pad, pad);
        mText = new TextView(this);
        mText.setTextSize(16);
        File dir = getExternalFilesDir(null);
        mText.setText((problem != null ? problem + "\n\n" : "")
            + getString(R.string.app_name) + " needs the disc image (.iso) of your own disc.\n\n"
            + "Choose it below, or copy it to\n" + (dir != null ? dir.getAbsolutePath() : "the app's folder")
            + "/" + BuildConfig.GAME + ".iso\n(for example: adb push game.iso " + (dir != null ? dir.getAbsolutePath() : "") + "/"
            + BuildConfig.GAME + ".iso) and start the app again.");
        Button pick = new Button(this);
        pick.setText("Choose the disc image");
        pick.setOnClickListener(v -> pick());
        box.addView(mText);
        box.addView(pick);
        setContentView(box);
    }

    private void pick() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        try { startActivityForResult(i, REQ_PICK); }
        catch (Exception e) { showChooser("This phone has no file picker: copy the image to the app's folder instead."); }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        if (request != REQ_PICK) { super.onActivityResult(request, result, data); return; }
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception e) {
            Log.w(TAG, "no lasting permission for " + uri + ": " + e);   // it still works for this start
        }
        String disc = openUri(this, uri);
        if (disc == null) { showChooser("That file cannot be read. Choose a disc image on the phone itself (not in the cloud)."); return; }
        SharedPreferences.Editor e = getSharedPreferences(PREFS, MODE_PRIVATE).edit();
        e.putString(KEY_DISC, uri.toString());
        e.apply();
        startGame(disc);
    }

    private void startGame(String disc) {
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra("disc", disc);
        Intent mine = getIntent();
        if (mine != null) {                   // testing: am start -n <app id>/ps2native.app.LauncherActivity -e args "..." -e env "..."
            if (mine.hasExtra("args")) i.putExtra("args", mine.getStringExtra("args"));
            if (mine.hasExtra("env")) i.putExtra("env", mine.getStringExtra("env"));
        }
        startActivity(i);
        finish();
    }
}
