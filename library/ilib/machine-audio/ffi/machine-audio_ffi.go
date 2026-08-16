// AC ilib: machine-audio — Go FFI (subprocess-based, no CGO needed)
package main

import (
	"fmt"
	"os/exec"
	"time"
)

func _maudio_which(cmd string) bool {
	_, err := exec.LookPath(cmd)
	return err == nil
}

func maudio_say(text string) bool { return maudio_say_rate(text, 175) }
func maudio_say_rate(text string, rate int64) bool {
	var cmd *exec.Cmd
	if _, err := exec.LookPath("espeak-ng"); err == nil {
		cmd = exec.Command("espeak-ng", "-s", fmt.Sprint(rate), text)
	} else if _, err := exec.LookPath("say"); err == nil {
		cmd = exec.Command("say", text)
	} else if _, err := exec.LookPath("espeak"); err == nil {
		cmd = exec.Command("espeak", "-s", fmt.Sprint(rate), text)
	} else {
		fmt.Printf("[machine-audio] %s\n", text); return false
	}
	return cmd.Run() == nil
}

func maudio_say_async(text string) { go maudio_say(text) }

func maudio_stop() { exec.Command("pkill", "-f", "espeak").Run() }

func maudio_listen(timeout_ms int64) string {
	secs := timeout_ms / 1000
	if secs < 1 { secs = 1 }
	if _, err := exec.LookPath("vosk-transcriber"); err == nil {
		ctx := exec.Command("sh", "-c", fmt.Sprintf(
			"arecord -d %d -f cd /tmp/_ac_audio.wav 2>/dev/null && vosk-transcriber /tmp/_ac_audio.wav 2>/dev/null", secs))
		out, err := ctx.Output()
		if err == nil && len(out) > 0 { return string(out) }
	}
	time.Sleep(time.Duration(secs) * time.Second)
	fmt.Println("[machine-audio] listen not available (install vosk-transcriber)")
	return ""
}

func maudio_play(path string) {
	if _, err := exec.LookPath("aplay"); err == nil {
		go exec.Command("aplay", path).Run()
	} else if _, err := exec.LookPath("paplay"); err == nil {
		go exec.Command("paplay", path).Run()
	}
}

// tts_ok() — is a real TTS engine available?
func maudio_tts_ok() bool {
	return _maudio_which("espeak-ng") || _maudio_which("say") || _maudio_which("espeak")
}

// AC's actual calling convention for this ilib is the DOTTED form (`maudio.speak(...)`,
// matching machine-audio.py's own `class maudio` staticmethod wrapper and every other native
// ilib implementation) — including the compiler's auto-injected `maudio.stop()` shutdown call.
var maudio = struct {
	speak  func(string) bool
	listen func(int64) string
	tts_ok func() bool
	stop   func()
}{maudio_say, maudio_listen, maudio_tts_ok, maudio_stop}
