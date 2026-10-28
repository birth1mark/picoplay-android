package com.birth1mark.picoplay;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

public class PicoPlayActivity extends SDLActivity {
    private static final String TAG = "PicoPlay";
    private static final int REQ_IMPORT_CART = 8008;
    private static PicoPlayActivity sInstance = null;

    @Override
    protected String[] getLibraries() {
        return new String[]{
            "SDL2",
            "main"
        };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        sInstance = this;
        extractStarterCarts();
        super.onCreate(savedInstanceState);
        handleIncomingIntent(getIntent());
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        handleIncomingIntent(intent);
    }

    private void extractStarterCarts() {
        try {
            File cartsDir = new File(getFilesDir(), "carts");
            if (!cartsDir.exists()) {
                cartsDir.mkdirs();
            }
            String[] assetFiles = getAssets().list("carts");
            if (assetFiles == null) return;

            for (String filename : assetFiles) {
                if (!filename.endsWith(".p8") && !filename.endsWith(".png")) continue;
                File outFile = new File(cartsDir, filename);
                if (!outFile.exists() || outFile.length() == 0) {
                    try (InputStream in = getAssets().open("carts/" + filename);
                         FileOutputStream out = new FileOutputStream(outFile)) {
                        byte[] buf = new byte[8192];
                        int len;
                        while ((len = in.read(buf)) > 0) {
                            out.write(buf, 0, len);
                        }
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to extract starter cartridges", e);
        }
    }

    private void handleIncomingIntent(Intent intent) {
        if (intent == null) return;
        if (Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getData() != null) {
            importCartridgeFromUri(intent.getData());
        }
    }

    public static void openImportCartridgePicker() {
        final PicoPlayActivity activity = sInstance;
        if (activity == null) return;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    activity.startActivityForResult(intent, REQ_IMPORT_CART);
                } catch (Exception e) {
                    Log.e(TAG, "Failed to open document picker", e);
                    Toast.makeText(activity, "Could not open file picker", Toast.LENGTH_SHORT).show();
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_IMPORT_CART && resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
            importCartridgeFromUri(data.getData());
        }
    }

    private void importCartridgeFromUri(Uri uri) {
        try {
            String displayName = "imported_cart.p8.png";
            try (Cursor cursor = getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int nameIdx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (nameIdx >= 0) {
                        String candidate = cursor.getString(nameIdx);
                        if (candidate != null && !candidate.trim().isEmpty()) {
                            displayName = candidate.trim().replaceAll("[^a-zA-Z0-9._-]", "_");
                        }
                    }
                }
            }
            String lower = displayName.toLowerCase();
            if (!lower.endsWith(".p8") && !lower.endsWith(".p8.png")) {
                if (lower.endsWith(".png")) {
                    displayName = displayName.substring(0, displayName.length() - 4) + ".p8.png";
                } else {
                    displayName = displayName + ".p8.png";
                }
            }

            File cartsDir = new File(getFilesDir(), "carts");
            if (!cartsDir.exists()) cartsDir.mkdirs();
            File destFile = new File(cartsDir, displayName);

            try (InputStream in = getContentResolver().openInputStream(uri);
                 FileOutputStream out = new FileOutputStream(destFile)) {
                if (in == null) return;
                byte[] buf = new byte[8192];
                int len;
                while ((len = in.read(buf)) > 0) {
                    out.write(buf, 0, len);
                }
            }

            final String savedName = displayName;
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(PicoPlayActivity.this, "Imported: " + savedName, Toast.LENGTH_SHORT).show();
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Failed to import cartridge", e);
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(PicoPlayActivity.this, "Failed to import cartridge", Toast.LENGTH_SHORT).show();
                }
            });
        }
    }

    public static boolean downloadUrlToFile(String urlStr, String destPath) {
        HttpURLConnection conn = null;
        try {
            URL url = new URL(urlStr);
            conn = (HttpURLConnection) url.openConnection();
            conn.setInstanceFollowRedirects(true);
            conn.setConnectTimeout(15000);
            conn.setReadTimeout(15000);
            conn.setRequestProperty("User-Agent", "PicoPlay/1.0 (Android; by birth1mark)");
            conn.connect();

            int code = conn.getResponseCode();
            if (code != HttpURLConnection.HTTP_OK) {
                Log.e(TAG, "HTTP error code: " + code + " for " + urlStr);
                return false;
            }

            File outFile = new File(destPath);
            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists()) {
                parent.mkdirs();
            }

            try (InputStream in = conn.getInputStream();
                 FileOutputStream out = new FileOutputStream(outFile)) {
                byte[] buf = new byte[16384];
                int read;
                while ((read = in.read(buf)) != -1) {
                    out.write(buf, 0, read);
                }
            }
            return true;
        } catch (Exception e) {
            Log.e(TAG, "Download failed: " + urlStr, e);
            return false;
        } finally {
            if (conn != null) {
                conn.disconnect();
            }
        }
    }
}
