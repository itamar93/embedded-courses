package main

import (
	"bufio"
	"context"
	"encoding/json"
	"fmt"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
)

// Event is what the page receives while firmware runs.
type Event struct {
	Type string `json:"type"`           // "uart", "pin", "exit"
	Data string `json:"data,omitempty"` // uart text
	Pin  string `json:"pin,omitempty"`
	V    int    `json:"v"`
	T    int64  `json:"t"` // virtual time, ns
}

// Session is one running QEMU with its UART, pin monitor and HMP monitor sockets.
type Session struct {
	cmd    *exec.Cmd
	dir    string
	uart   net.Conn
	pins   net.Conn
	mon    net.Conn
	monRd  *bufio.Reader
	monMu  sync.Mutex
	Events chan Event
	done   chan struct{}
	once   sync.Once
}

func dialRetry(ctx context.Context, path string) (net.Conn, error) {
	for {
		c, err := net.Dial("unix", path)
		if err == nil {
			return c, nil
		}
		select {
		case <-ctx.Done():
			return nil, fmt.Errorf("emulator did not start: %w", err)
		case <-time.After(20 * time.Millisecond):
		}
	}
}

// StartQEMU boots elf on the board and streams its UART and pin events.
func StartQEMU(ctx context.Context, qemu, board, elf string) (*Session, error) {
	dir, err := os.MkdirTemp("", "ecr-")
	if err != nil {
		return nil, err
	}
	sock := func(n string) string { return filepath.Join(dir, n) }
	args := []string{
		"-M", board, "-display", "none", "-nodefaults",
		"-chardev", "socket,id=mon,path=" + sock("mon") + ",server=on,wait=off",
		"-mon", "chardev=mon,mode=readline",
		"-chardev", "socket,id=uart,path=" + sock("uart") + ",server=on,wait=on",
		"-chardev", "socket,id=pins,path=" + sock("pins") + ",server=on,wait=on",
		// USART2 is the second serial port of the STM32F4 SoC
		"-serial", "null", "-serial", "chardev:uart",
		"-kernel", elf,
	}
	cmd := exec.Command(qemu, args...)
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		os.RemoveAll(dir)
		return nil, err
	}
	s := &Session{cmd: cmd, dir: dir, Events: make(chan Event, 1024), done: make(chan struct{})}

	dctx, cancel := context.WithTimeout(ctx, 5*time.Second)
	defer cancel()
	if s.uart, err = dialRetry(dctx, sock("uart")); err == nil {
		if s.pins, err = dialRetry(dctx, sock("pins")); err == nil {
			s.mon, err = dialRetry(dctx, sock("mon"))
		}
	}
	if err != nil {
		s.Stop()
		return nil, err
	}
	s.monRd = bufio.NewReader(s.mon)
	s.readUntilPrompt()

	var wg sync.WaitGroup
	wg.Add(2)
	go func() { defer wg.Done(); s.pumpUART() }()
	go func() { defer wg.Done(); s.pumpPins() }()
	go func() {
		cmd.Wait()
		wg.Wait()
		s.Events <- Event{Type: "exit"}
		close(s.Events)
		s.Stop()
	}()
	return s, nil
}

func (s *Session) pumpUART() {
	buf := make([]byte, 512)
	for {
		n, err := s.uart.Read(buf)
		if n > 0 {
			s.emit(Event{Type: "uart", Data: string(buf[:n])})
		}
		if err != nil {
			return
		}
	}
}

func (s *Session) pumpPins() {
	sc := bufio.NewScanner(s.pins)
	for sc.Scan() {
		var e Event
		if json.Unmarshal(sc.Bytes(), &e) == nil {
			e.Type = "pin"
			s.emit(e)
		}
	}
}

// emit drops events instead of blocking QEMU when nobody is reading fast enough.
func (s *Session) emit(e Event) {
	select {
	case s.Events <- e:
	default:
	}
}

// SetPin drives an input pin, for example the KEY button: SetPin("PA0", 0).
func (s *Session) SetPin(pin string, level int) error {
	if !regexp.MustCompile(`^P[A-EH]([0-9]|1[0-5])$`).MatchString(pin) {
		return fmt.Errorf("bad pin %q", pin)
	}
	_, err := fmt.Fprintf(s.pins, "%s=%d\n", pin, level)
	return err
}

// SendUART types text into USART2.
func (s *Session) SendUART(text string) error {
	_, err := s.uart.Write([]byte(text))
	return err
}

func (s *Session) readUntilPrompt() string {
	var sb strings.Builder
	s.mon.SetReadDeadline(time.Now().Add(2 * time.Second))
	defer s.mon.SetReadDeadline(time.Time{})
	for {
		b, err := s.monRd.ReadByte()
		if err != nil {
			return sb.String()
		}
		sb.WriteByte(b)
		if strings.HasSuffix(sb.String(), "(qemu) ") {
			return sb.String()
		}
	}
}

var xpLine = regexp.MustCompile(`(?m)^[0-9a-f]+:\s+0x([0-9a-f]+)`)

// ReadWord reads a 32-bit word of guest physical memory through the HMP monitor.
func (s *Session) ReadWord(addr uint32) (uint32, error) {
	s.monMu.Lock()
	defer s.monMu.Unlock()
	if _, err := fmt.Fprintf(s.mon, "xp /1wx 0x%08x\n", addr); err != nil {
		return 0, err
	}
	out := s.readUntilPrompt()
	m := xpLine.FindStringSubmatch(out)
	if m == nil {
		return 0, fmt.Errorf("monitor: unexpected reply %q", out)
	}
	v, err := strconv.ParseUint(m[1], 16, 32)
	return uint32(v), err
}

// Pause freezes the guest so registers can be read in a consistent state.
func (s *Session) Pause() {
	s.monMu.Lock()
	defer s.monMu.Unlock()
	fmt.Fprint(s.mon, "stop\n")
	s.readUntilPrompt()
}

func (s *Session) Done() <-chan struct{} { return s.done }

func (s *Session) Stop() {
	s.once.Do(func() {
		if s.cmd.Process != nil {
			s.cmd.Process.Kill()
		}
		for _, c := range []net.Conn{s.uart, s.pins, s.mon} {
			if c != nil {
				c.Close()
			}
		}
		os.RemoveAll(s.dir)
		close(s.done)
	})
}
