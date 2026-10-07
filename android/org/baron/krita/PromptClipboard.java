// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;

public final class PromptClipboard {
    private PromptClipboard() { }

    public static void writeText(Context context, String text) {
        if (context == null || text == null) return;
        try {
            ClipboardManager clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard != null) clipboard.setPrimaryClip(ClipData.newPlainText("Krita prompt", text));
        } catch (RuntimeException error) { }
    }

    public static String readText(Context context) {
        if (context == null) return "";
        try {
            ClipboardManager clipboard = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard == null) return "";
            ClipData clip = clipboard.getPrimaryClip();
            if (clip == null || clip.getItemCount() == 0) return "";
            CharSequence text = clip.getItemAt(0).getText();
            return text == null ? "" : text.toString();
        } catch (RuntimeException error) {
            return "";
        }
    }
}
