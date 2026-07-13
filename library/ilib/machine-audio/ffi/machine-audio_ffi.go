// AC ilib: machine-audio — Go FFI (subprocess-based, no CGO needed)
package main

import (
	"fmt"
	"os/exec"
	"time"
)

func _maudio_which(cmd string) bool {
	return exec.LookPath(cmd) == nil || func() bool { _, err := exec.LookPath(cmd); return err == nil }()
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
