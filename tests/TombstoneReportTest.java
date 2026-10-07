package org.baron.krita;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;

public final class TombstoneReportTest {
    private static byte[] varint(long value) {
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        do {
            int next = (int) value & 127;
            value >>>= 7;
            data.write(next | (value == 0 ? 0 : 128));
        } while (value != 0);
        return data.toByteArray();
    }
    private static byte[] join(byte[]... parts) {
        ByteArrayOutputStream data = new ByteArrayOutputStream();
        for (byte[] part : parts) data.write(part, 0, part.length);
        return data.toByteArray();
    }
    private static byte[] number(int field, long value) { return join(varint(field << 3), varint(value)); }
    private static byte[] bytes(int field, byte[] value) { return join(varint((field << 3) | 2), varint(value.length), value); }
    private static byte[] text(int field, String value) { return bytes(field, value.getBytes(StandardCharsets.UTF_8)); }
    public static void main(String[] args) {
        byte[] frame = join(number(1, 0x1234), text(4, "QTextLayout::draw"), number(5, 12),
            text(6, "libQt5Gui_arm64-v8a.so"), text(8, "abcdef"));
        byte[] thread = join(number(1, 42), text(2, "QtThread"), bytes(4, frame), text(5, "PRIVATE_MEMORY"));
        byte[] trace = join(number(6, 42), bytes(10, join(number(1, 11), text(2, "SIGSEGV"), number(9, -1))),
            bytes(16, join(number(1, 99), bytes(2, text(2, "OtherThread")))),
            bytes(16, join(number(1, 42), bytes(2, thread))), text(18, "PRIVATE_LOG"), text(14, "PRIVATE_ABORT"));
        String report = TombstoneReport.format(trace);
        if (!report.contains("SIGSEGV (11)") || !report.contains("#0 pc 1234 libQt5Gui_arm64-v8a.so")) throw new AssertionError(report);
        if (!report.contains("QTextLayout::draw+12") || !report.contains("fault=0xffffffffffffffff")) throw new AssertionError(report);
        if (report.contains("PRIVATE") || report.contains("OtherThread")) throw new AssertionError("Unexpected private fields");
        byte[][] malformed = { {0}, {8, (byte) 128}, {10, 10, 0}, {(byte) 15}, new byte[4 * 1024 * 1024 + 1] };
        for (byte[] data : malformed) {
            boolean rejected = false;
            try { TombstoneReport.format(data); } catch (IllegalArgumentException expected) { rejected = true; }
            if (!rejected) throw new AssertionError("Accepted malformed input");
        }
        System.out.println("Tombstone parser: stack, crashed thread, unsigned address, privacy and 5 malformed cases passed");
    }
}
