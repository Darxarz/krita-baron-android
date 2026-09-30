// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
public final class Credentials {
    private static final String ALIAS = "baron_orchestrion_device_v1";
    private static SecretKey key() throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey) store.getKey(ALIAS, null);
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM).setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true).build());
        return generator.generateKey();
    }
    public static String seal(String value) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding"); cipher.init(Cipher.ENCRYPT_MODE, key());
            byte[] encrypted = cipher.doFinal(value.getBytes(StandardCharsets.UTF_8)), iv = cipher.getIV();
            byte[] output = new byte[iv.length + encrypted.length];
            System.arraycopy(iv, 0, output, 0, iv.length); System.arraycopy(encrypted, 0, output, iv.length, encrypted.length);
            return Base64.encodeToString(output, Base64.NO_WRAP);
        } catch (Exception ignored) { return ""; }
    }
    public static String unseal(String value) {
        if (value == null || value.isEmpty()) return "";
        try {
            byte[] input = Base64.decode(value, Base64.NO_WRAP); if (input.length <= 28) return "";
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(), new GCMParameterSpec(128, Arrays.copyOfRange(input, 0, 12)));
            return new String(cipher.doFinal(Arrays.copyOfRange(input, 12, input.length)), StandardCharsets.UTF_8);
        } catch (Exception ignored) { return ""; }
    }
    private Credentials() {}
}
