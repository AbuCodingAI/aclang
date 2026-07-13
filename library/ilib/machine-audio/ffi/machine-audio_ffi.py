import subprocess as _sp, shutil as _sh, tempfile as _tf, os as _os

_audio_procs = []

def audio_speak(text, rate=175):
    if _sh.which("espeak-ng"):
        _sp.run(["espeak-ng", "-s", str(rate), str(text)], check=False)
        return True
    elif _sh.which("say"):
        _sp.run(["say", str(text)], check=False)
        return True
    elif _sh.which("espeak"):
        _sp.run(["espeak", "-s", str(rate), str(text)], check=False)
        return True
    elif _sh.which("spd-say"):
        _sp.run(["spd-say", "-r", str(rate - 175), str(text)], check=False)
        return True
    else:
        print(f"[machine-audio] {text}")
        return False

def audio_speak_async(text, rate=175):
    proc = None
    if _sh.which("espeak-ng"):
        proc = _sp.Popen(["espeak-ng", "-s", str(rate), str(text)])
    elif _sh.which("say"):
        proc = _sp.Popen(["say", str(text)])
    elif _sh.which("spd-say"):
        proc = _sp.Popen(["spd-say", "-r", str(rate - 175), str(text)])
    else:
        print(f"[machine-audio async] {text}")
    if proc:
        _audio_procs.append(proc)
        _audio_procs[:] = [p for p in _audio_procs if p.poll() is None]

def audio_stop():
    for proc in _audio_procs:
        if proc.poll() is None:
            proc.terminate()
    _audio_procs.clear()

def audio_listen(timeout_ms=5000):
    secs = max(1, int(timeout_ms) // 1000)
    try:
        import speech_recognition as _sr
        r = _sr.Recognizer()
        with _sr.Microphone() as src:
            r.adjust_for_ambient_noise(src, duration=0.3)
            audio = r.listen(src, timeout=secs)
        return r.recognize_google(audio)
    except Exception:
        pass
    tmp = _tf.mktemp(suffix=".wav")
    try:
        _sp.run(["arecord", "-d", str(secs), "-f", "cd", "-t", "wav", tmp],
                check=False, capture_output=True)
        out_base = tmp + "_out"
        _sp.run(["whisper", "--model", "tiny", tmp,
                 "--output-txt", "--output-file", out_base],
                capture_output=True, check=False)
        if _os.path.exists(out_base + ".txt"):
            with open(out_base + ".txt") as f:
                return f.read().strip()
    finally:
        for p in [tmp, tmp + "_out.txt"]:
            try: _os.remove(p)
            except: pass
    return ""

def audio_tts_ok():
    return bool(_sh.which("espeak-ng") or _sh.which("say") or
                _sh.which("espeak") or _sh.which("spd-say"))

def audio_stt_ok():
    try:
        import speech_recognition; return True
    except ImportError: pass
    return bool(_sh.which("whisper"))

class maudio:
    speak       = staticmethod(audio_speak)
    speak_async = staticmethod(audio_speak_async)
    stop        = staticmethod(audio_stop)
    listen      = staticmethod(audio_listen)
    tts_ok      = staticmethod(audio_tts_ok)
    stt_ok      = staticmethod(audio_stt_ok)
