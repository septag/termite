package com.termite.utils;

import android.content.res.AssetManager;

/**
 * Created by sepul on 10/2/2016.
 */

public class PlatformUtils
{
    public static native void termiteInitAssetManager(AssetManager assetManager);
    public static native void termiteInitPaths(String dataDir, String cacheDir);
}
