// AC ilib: machine-audio — Speech-to-Text and Text-to-Speech (JavaScript/Node.js)
const { execSync, spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

/**
 * maudio_speak(text, rate) - Speak text aloud
 * rate: 50-300 (default 175)
 * Returns: true if successful, false if no TTS available
 */
function maudio_speak(text, rate = 175) {
    if (!text) return false;
    rate = Math.max(50, Math.min(300, rate || 175));

    try {
        // Try espeak-ng first (Linux/cross-platform)
        if (commandExists('espeak-ng')) {
            execSync(`espeak-ng -s ${rate} "${text.replace(/"/g, '\\"')}"`, { stdio: 'pipe' });
            return true;
        }
        // Try say (macOS)
        if (commandExists('say')) {
            execSync(`say "${text.replace(/"/g, '\\"')}"`, { stdio: 'pipe' });
            return true;
        }
        // Try espeak (Linux)
        if (commandExists('espeak')) {
            execSync(`espeak -s ${rate} "${text.replace(/"/g, '\\"')}"`, { stdio: 'pipe' });
            return true;
        }
        // Try spd-say (Linux)
        if (commandExists('spd-say')) {
            execSync(`spd-say -r ${rate - 175} "${text.replace(/"/g, '\\"')}"`, { stdio: 'pipe' });
            return true;
        }
        // Fallback
        console.log(`[machine-audio::speak] ${text}`);
        return false;
    } catch (err) {
        console.log(`[machine-audio::speak] ${text}`);
        return false;
    }
}

/**
 * maudio_speak_async(text, rate) - Speak text without blocking
 * Spawns TTS in background
 */
function maudio_speak_async(text, rate = 175) {
    if (!text) return;
    rate = Math.max(50, Math.min(300, rate || 175));

    try {
        if (commandExists('espeak-ng')) {
            require('child_process').spawn('espeak-ng', ['-s', String(rate), text]);
        } else if (commandExists('say')) {
            require('child_process').spawn('say', [text]);
        } else if (commandExists('espeak')) {
            require('child_process').spawn('espeak', ['-s', String(rate), text]);
        } else if (commandExists('spd-say')) {
            require('child_process').spawn('spd-say', ['-r', String(rate - 175), text]);
        } else {
            console.log(`[machine-audio::speak_async] ${text}`);
        }
    } catch (err) {
        console.log(`[machine-audio::speak_async] ${text}`);
    }
}

/**
 * maudio_listen(timeout_ms) - Record and transcribe from microphone
 * Returns: transcribed text
 */
function maudio_listen(timeout_ms = 5000) {
    timeout_ms = Math.max(1000, timeout_ms || 5000);
    const secs = Math.ceil(timeout_ms / 1000);

    // Try using whisper CLI (Requires: pip install openai-whisper)
    try {
        const tmpFile = path.join(os.tmpdir(), `maudio_${Date.now()}.wav`);
        const outFile = tmpFile + '_out';

        // Record audio using ffmpeg or arecord
        if (commandExists('arecord')) {
            execSync(`arecord -d ${secs} -f cd -t wav "${tmpFile}"`, { stdio: 'pipe' });
        } else if (commandExists('ffmpeg')) {
            execSync(`ffmpeg -f avfoundation -i ":0" -t ${secs} "${tmpFile}" -y`, {
                stdio: 'pipe',
                cwd: os.tmpdir()
            });
        } else {
            return '';
        }

        // Transcribe using whisper
        if (commandExists('whisper')) {
            execSync(`whisper --model tiny "${tmpFile}" --output-txt --output-file "${outFile}"`, {
                stdio: 'pipe'
            });

            const txtPath = outFile + '.txt';
            if (fs.existsSync(txtPath)) {
                const text = fs.readFileSync(txtPath, 'utf-8').trim();
                // Cleanup
                try {
                    fs.unlinkSync(tmpFile);
                    fs.unlinkSync(txtPath);
                } catch (e) {}
                return text;
            }
        }

        // Cleanup on failure
        try { fs.unlinkSync(tmpFile); } catch (e) {}
        return '';
    } catch (err) {
        return '';
    }
}

/**
 * maudio_tts_ok() - Check if Text-to-Speech is available
 * Returns: 1 if TTS available, 0 otherwise
 */
function maudio_tts_ok() {
    return commandExists('espeak-ng') || commandExists('say') ||
           commandExists('espeak') || commandExists('spd-say') ? 1 : 0;
}

/**
 * maudio_stt_ok() - Check if Speech-to-Text is available
 * Returns: 1 if STT available, 0 otherwise
 */
function maudio_stt_ok() {
    return commandExists('whisper') ? 1 : 0;
}

/**
 * Helper: Check if command exists
 */
function commandExists(cmd) {
    try {
        if (process.platform === 'win32') {
            execSync(`where ${cmd}`, { stdio: 'pipe' });
        } else {
            execSync(`which ${cmd}`, { stdio: 'pipe' });
        }
        return true;
    } catch {
        return false;
    }
}

// Export for Node.js
module.exports = {
    maudio_speak,
    maudio_speak_async,
    maudio_listen,
    maudio_tts_ok,
    maudio_stt_ok
};
