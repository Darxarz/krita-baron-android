// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import java.io.File;
import java.util.Arrays;

public final class Updates {
    public static String installedName(Context context) {
        try { return context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionName; }
        catch (Exception ignored) { return ""; }
    }
    public static int installedCode(Context context) {
        try { return context.getPackageManager().getPackageInfo(context.getPackageName(), 0).versionCode; }
        catch (Exception ignored) { return -1; }
    }
    public static String directory(Context context) {
        return new File(context.getCacheDir(), "baron-updates").getAbsolutePath();
    }
    private static Signature[] signatures(PackageInfo info) {
        if (Build.VERSION.SDK_INT >= 28 && info.signingInfo != null)
            return info.signingInfo.getApkContentsSigners();
        return info.signatures;
    }
    public static int install(Activity activity, String path, int expectedCode) {
        try {
            File file = new File(path).getCanonicalFile();
            File root = new File(directory(activity)).getCanonicalFile();
            if (!file.isFile() || !file.getParentFile().equals(root) || !file.getName().endsWith(".apk")) return 2;
            PackageManager manager = activity.getPackageManager();
            int flags = Build.VERSION.SDK_INT >= 28 ? PackageManager.GET_SIGNING_CERTIFICATES : PackageManager.GET_SIGNATURES;
            PackageInfo update = manager.getPackageArchiveInfo(file.getPath(), flags);
            PackageInfo current = manager.getPackageInfo(activity.getPackageName(), flags);
            if (update == null || !current.packageName.equals(update.packageName)
                || update.versionCode != expectedCode || expectedCode <= current.versionCode) return 2;
            Signature[] oldSigners = signatures(current), newSigners = signatures(update);
            if (oldSigners == null || newSigners == null || oldSigners.length != newSigners.length) return 2;
            for (Signature signer : oldSigners) {
                boolean found = false;
                for (Signature other : newSigners) if (Arrays.equals(signer.toByteArray(), other.toByteArray())) found = true;
                if (!found) return 2;
            }
            if (Build.VERSION.SDK_INT >= 26 && !manager.canRequestPackageInstalls()) {
                activity.runOnUiThread(() -> activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + activity.getPackageName()))));
                return 1;
            }
            activity.getSharedPreferences("BaronUpdates", Context.MODE_PRIVATE).edit().putString("apk", file.getPath()).commit();
            Uri uri = Uri.parse("content://" + activity.getPackageName() + ".baronupdates/update.apk");
            activity.runOnUiThread(() -> activity.startActivity(new Intent(Intent.ACTION_VIEW)
                .setDataAndType(uri, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)));
            return 0;
        } catch (Exception ignored) { return 2; }
    }
    private Updates() {}
}
