// AC ilib: machine-audio — Speech-to-Text and Text-to-Speech (Java)
import java.io.*;
import java.nio.file.*;

class MachineAudio {

    /**
     * maudio_speak - Speak text aloud
     * @param text Text to speak
     * @param rate Speech rate (50-300, default 175)
     * @return true if successful
     */
    public static boolean maudioSpeak(String text, int rate) {
        if (text == null || text.isEmpty()) return false;
        rate = Math.max(50, Math.min(300, rate > 0 ? rate : 175));

        try {
            // Try espeak-ng
            if (commandExists("espeak-ng")) {
                ProcessBuilder pb = new ProcessBuilder("espeak-ng", "-s", String.valueOf(rate), text);
                Process p = pb.start();
                p.waitFor();
                return true;
            }
            // Try say (macOS)
            if (commandExists("say")) {
                ProcessBuilder pb = new ProcessBuilder("say", text);
                Process p = pb.start();
                p.waitFor();
                return true;
            }
            // Try espeak
            if (commandExists("espeak")) {
                ProcessBuilder pb = new ProcessBuilder("espeak", "-s", String.valueOf(rate), text);
                Process p = pb.start();
                p.waitFor();
                return true;
            }
            // Try spd-say
            if (commandExists("spd-say")) {
                ProcessBuilder pb = new ProcessBuilder("spd-say", "-r", String.valueOf(rate - 175), text);
                Process p = pb.start();
                p.waitFor();
                return true;
            }

            System.out.println("[machine-audio::speak] " + text);
            return false;
        } catch (Exception e) {
            System.out.println("[machine-audio::speak] " + text);
            return false;
        }
    }

    /**
     * maudio_speak - Speak text aloud (default rate)
     */
    public static boolean maudioSpeak(String text) {
        return maudioSpeak(text, 175);
    }

    /**
     * maudio_speak_async - Speak text without blocking
     */
    public static void maudioSpeakAsync(String text, int rate) {
        if (text == null || text.isEmpty()) return;
        rate = Math.max(50, Math.min(300, rate > 0 ? rate : 175));

        final int finalRate = rate;
        final String finalText = text;

        new Thread(() -> {
            try {
                if (commandExists("espeak-ng")) {
                    new ProcessBuilder("espeak-ng", "-s", String.valueOf(finalRate), finalText).start();
                } else if (commandExists("say")) {
                    new ProcessBuilder("say", finalText).start();
                } else if (commandExists("espeak")) {
                    new ProcessBuilder("espeak", "-s", String.valueOf(finalRate), finalText).start();
                } else if (commandExists("spd-say")) {
                    new ProcessBuilder("spd-say", "-r", String.valueOf(finalRate - 175), finalText).start();
                } else {
                    System.out.println("[machine-audio::speak_async] " + finalText);
                }
            } catch (IOException e) {
                System.out.println("[machine-audio::speak_async] " + finalText);
            }
        }).start();
    }

    /**
     * maudio_speak_async - Speak text without blocking (default rate)
     */
    public static void maudioSpeakAsync(String text) {
        maudioSpeakAsync(text, 175);
    }

    /**
     * maudio_listen - Record from microphone and transcribe
     * @param timeoutMs Timeout in milliseconds
     * @return Transcribed text
     */
    public static String maudioListen(int timeoutMs) {
        timeoutMs = Math.max(1000, timeoutMs > 0 ? timeoutMs : 5000);
        int secs = (timeoutMs + 999) / 1000;

        try {
            String tmpFile = Files.createTempFile("maudio_", ".wav").toString();
            String outFile = tmpFile + "_out";

            // Record audio
            boolean recorded = false;
            if (commandExists("arecord")) {
                ProcessBuilder pb = new ProcessBuilder(
                    "arecord", "-d", String.valueOf(secs), "-f", "cd", "-t", "wav", tmpFile
                );
                Process p = pb.start();
                p.waitFor();
                recorded = Files.exists(Paths.get(tmpFile));
            } else if (commandExists("ffmpeg")) {
                ProcessBuilder pb = new ProcessBuilder(
                    "ffmpeg", "-f", "avfoundation", "-i", ":0", "-t", String.valueOf(secs),
                    tmpFile, "-y"
                );
                Process p = pb.start();
                p.waitFor();
                recorded = Files.exists(Paths.get(tmpFile));
            }

            if (!recorded) return "";

            // Transcribe with whisper
            if (commandExists("whisper")) {
                ProcessBuilder pb = new ProcessBuilder(
                    "whisper", "--model", "tiny", tmpFile,
                    "--output-txt", "--output-file", outFile
                );
                Process p = pb.start();
                p.waitFor();

                String txtPath = outFile + ".txt";
                if (Files.exists(Paths.get(txtPath))) {
                    String text = new String(Files.readAllBytes(Paths.get(txtPath))).trim();
                    // Cleanup
                    try {
                        Files.deleteIfExists(Paths.get(tmpFile));
                        Files.deleteIfExists(Paths.get(txtPath));
                    } catch (Exception e) {}
                    return text;
                }
            }

            // Cleanup on failure
            try { Files.deleteIfExists(Paths.get(tmpFile)); } catch (Exception e) {}
            return "";
        } catch (Exception e) {
            return "";
        }
    }

    /**
     * maudio_tts_ok - Check if Text-to-Speech is available
     * @return 1 if available, 0 otherwise
     */
    public static int maudioTtsOk() {
        return (commandExists("espeak-ng") || commandExists("say") ||
                commandExists("espeak") || commandExists("spd-say")) ? 1 : 0;
    }

    /**
     * maudio_stt_ok - Check if Speech-to-Text is available
     * @return 1 if available, 0 otherwise
     */
    public static int maudioSttOk() {
        return commandExists("whisper") ? 1 : 0;
    }

    /**
     * Helper: Check if command exists in PATH
     */
    private static boolean commandExists(String cmd) {
        String os = System.getProperty("os.name").toLowerCase();
        String[] cmd_array;

        try {
            if (os.contains("win")) {
                cmd_array = new String[]{"cmd.exe", "/c", "where " + cmd};
            } else {
                cmd_array = new String[]{"/bin/sh", "-c", "which " + cmd};
            }

            Process p = Runtime.getRuntime().exec(cmd_array);
            return p.waitFor() == 0;
        } catch (Exception e) {
            return false;
        }
    }
}

// maudio namespace class — AC-generated Java uses maudio.speak(...)/maudio.listen(...)/
// maudio.tts_ok(), etc. Missing entirely before (verified: examples/audio_test.ac,
// "cannot find symbol: variable maudio" — every other backend resolves `maudio.*` through a
// native object/namespace of some kind; Java had the real MachineAudio implementation but no
// dispatcher exposing it under the AC-level name, same gap `os`/`stringm` had — see
// os_ffi.java/string-cheese_ffi.java for the identical fix pattern).
class maudio {
    public static boolean speak(String text)             { return MachineAudio.maudioSpeak(text); }
    public static boolean speak(String text, long rate)  { return MachineAudio.maudioSpeak(text, (int) rate); }
    public static void speak_async(String text)          { MachineAudio.maudioSpeakAsync(text); }
    public static void speak_async(String text, long rate) { MachineAudio.maudioSpeakAsync(text, (int) rate); }
    public static String listen(long timeoutMs)           { return MachineAudio.maudioListen((int) timeoutMs); }
    // AC-level tts_ok/stt_ok are used as booleans (`IF maudio.tts_ok()`, `tts_ready = ...`) —
    // real boolean return, not MachineAudio's underlying int, so callers type-check correctly.
    public static boolean tts_ok()                        { return MachineAudio.maudioTtsOk() != 0; }
    public static boolean stt_ok()                        { return MachineAudio.maudioSttOk() != 0; }
    public static void stop()                              { /* no-op: no persistent playback handle to stop on this backend */ }
}
