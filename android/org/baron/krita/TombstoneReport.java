// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

// Reads only stack metadata from Android's public tombstone.proto; no memory or log buffers.
public final class TombstoneReport {
    private static final class Field {
        int number;
        long value;
        byte[] bytes = new byte[0];
        String text() {
            return new String(bytes, StandardCharsets.UTF_8).replace('\n', ' ').replace('\r', ' ');
        }
    }
    private static long varint(byte[] data, int[] cursor) {
        long value = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (cursor[0] >= data.length) throw new IllegalArgumentException("Truncated varint");
            int next = data[cursor[0]++] & 255;
            if (shift == 63 && (next & 254) != 0) throw new IllegalArgumentException("Varint overflow");
            value |= (long) (next & 127) << shift;
            if ((next & 128) == 0) return value;
        }
        throw new IllegalArgumentException("Varint overflow");
    }
    private static List<Field> fields(byte[] data) {
        if (data.length > 4 * 1024 * 1024) throw new IllegalArgumentException("Trace too large");
        List<Field> fields = new ArrayList<>();
        int[] cursor = {0};
        while (cursor[0] < data.length) {
            if (fields.size() >= 20000) throw new IllegalArgumentException("Too many fields");
            long tag = varint(data, cursor);
            if (tag <= 0 || tag >>> 3 > 536870911) throw new IllegalArgumentException("Invalid tag");
            Field field = new Field();
            field.number = (int) (tag >>> 3);
            int wire = (int) (tag & 7);
            if (wire == 0) field.value = varint(data, cursor);
            else {
                long length = wire == 1 ? 8 : wire == 5 ? 4 : wire == 2 ? varint(data, cursor) : -1;
                if (length < 0 || length > data.length - cursor[0]) throw new IllegalArgumentException("Invalid field length");
                // Discard raw memory, logs, mappings and unknown top-level payloads without decoding them.
                field.bytes = Arrays.copyOfRange(data, cursor[0], cursor[0] + (int) length);
                cursor[0] += (int) length;
            }
            fields.add(field);
        }
        return fields;
    }
    private static Field find(List<Field> fields, int number) {
        for (Field field : fields) if (field.number == number) return field;
        return new Field();
    }
    private static String limited(String value, int limit) {
        return value.length() <= limit ? value : value.substring(0, limit) + "…";
    }
    public static String format(byte[] data) {
        List<Field> top = fields(data);
        long tid = find(top, 6).value;
        List<Field> signal = fields(find(top, 10).bytes);
        StringBuilder result = new StringBuilder();
        result.append("Native signal: ").append(limited(find(signal, 2).text(), 80))
            .append(" (").append(find(signal, 1).value).append(") ")
            .append(limited(find(signal, 4).text(), 80))
            .append("; fault=0x").append(Long.toHexString(find(signal, 9).value)).append('\n');
        result.append("Crashed thread id: ").append(tid).append('\n');
        for (Field entry : top) {
            if (entry.number != 16) continue;
            List<Field> map = fields(entry.bytes);
            if (find(map, 1).value != tid) continue;
            List<Field> thread = fields(find(map, 2).bytes);
            result.append("Thread: ").append(limited(find(thread, 2).text(), 120)).append('\n');
            int index = 0;
            for (Field item : thread) {
                if (item.number != 4 || index >= 64) continue;
                List<Field> frame = fields(item.bytes);
                result.append('#').append(index++).append(" pc ")
                    .append(Long.toHexString(find(frame, 1).value)).append(' ')
                    .append(limited(find(frame, 6).text(), 512)).append(" (")
                    .append(limited(find(frame, 4).text(), 512)).append('+')
                    .append(find(frame, 5).value).append(") build-id=")
                    .append(limited(find(frame, 8).text(), 128)).append('\n');
            }
            break;
        }
        return result.toString();
    }
    private TombstoneReport() { }
}
