# AC ilib: machine-audio — Speech-to-Text and Text-to-Speech (Python backend)
import subprocess, shutil, tempfile, os

def maudio_speak(text, rate=175):
    """Speak text aloud using eSpeak-NG or system TTS."""
    if shutil.which("espeak-ng"):
        subprocess.run(["espeak-ng", "-s", str(rate), text], check=False)
        return True
    elif shutil.which("say"):  # macOS
        subprocess.run(["say", text], check=False)
        return True
    elif shutil.which("espeak"):
        subprocess.run(["espeak", "-s", str(rate), text], check=False)
        return True
    elif shutil.which("spd-say"):
        subprocess.run(["spd-say", "-r", str(rate - 175), text], check=False)
        return True
    else:
        print(f"[machine-audio::speak] {text}")
        return False

def maudio_speak_async(text, rate=175):
    """Speak text without blocking."""
    if shutil.which("espeak-ng"):
        subprocess.Popen(["espeak-ng", "-s", str(rate), text])
    elif shutil.which("say"):
        subprocess.Popen(["say", text])
    elif shutil.which("spd-say"):
        subprocess.Popen(["spd-say", "-r", str(rate - 175), text])
    else:
        print(f"[machine-audio::speak_async] {text}")

def maudio_listen(timeout_ms=5000):
    """Record from microphone and return transcribed text.
    Requires: arecord (Linux) + whisper CLI, or SpeechRecognition package."""
    secs = max(1, timeout_ms // 1000)
    # Try SpeechRecognition (pip install SpeechRecognition pyaudio)
    try:
        import speech_recognition as sr
        r = sr.Recognizer()
        with sr.Microphone() as src:
            r.adjust_for_ambient_noise(src, duration=0.3)
            audio = r.listen(src, timeout=secs)
        return r.recognize_google(audio)
    except Exception:
        pass
    # Fallback: arecord + whisper CLI
    tmp = tempfile.mktemp(suffix=".wav")
    try:
        subprocess.run(["arecord", "-d", str(secs), "-f", "cd", "-t", "wav", tmp],
                       check=False, capture_output=True)
        out = subprocess.run(["whisper", "--model", "tiny", tmp,
                              "--output-txt", "--output-file", tmp + "_out"],
                             capture_output=True, check=False)
        txt_path = tmp + "_out.txt"
        if os.path.exists(txt_path):
            with open(txt_path) as f: return f.read().strip()
    finally:
        for p in [tmp, tmp + "_out.txt"]:
            try: os.remove(p)
            except: pass
    return ""

def maudio_tts_ok():
    return bool(shutil.which("espeak-ng") or shutil.which("say") or shutil.which("espeak") or shutil.which("spd-say"))

def maudio_stt_ok():
    try:
        import speech_recognition; return True
    except ImportError: pass
    return bool(shutil.which("whisper"))
