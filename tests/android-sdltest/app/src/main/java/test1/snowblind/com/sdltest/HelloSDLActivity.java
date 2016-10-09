package test1.snowblind.com.sdltest;

import android.util.Log;
import android.os.Bundle;
import com.termite.utils.PlatformUtils;
import org.libsdl.app.SDLActivity;

/**
 * An example full-screen activity that shows and hides the system UI (i.e.
 * status bar and navigation/system bar) with user interaction.
 */
public class HelloSDLActivity extends SDLActivity {
    private static final String TAG = "Game";
    private static final String GAME_LIB = "test_sdl";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.v(TAG, "Start Activity");

        // Load game library
        try {
            System.loadLibrary(GAME_LIB);
            Log.v(TAG, "Loaded " + GAME_LIB + " library");
        } catch(UnsatisfiedLinkError e) {
            System.err.println(e.getMessage());
            return;
        } catch(Exception e) {
            System.err.println(e.getMessage());
            return;
        }

        PlatformUtils.termiteInitAssetManager(this.getAssets());
        PlatformUtils.termiteInitPaths(this.getApplicationInfo().dataDir, this.getCacheDir().getAbsolutePath());

        super.onCreate(savedInstanceState);
    }
}
