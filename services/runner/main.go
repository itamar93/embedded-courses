// Command runner builds lab firmware with arm-none-eabi-gcc and runs it in QEMU
// for the course website: compile, stream UART and pin events, and grade labs.
package main

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"log"
	"net/http"
	"os"
	"sync"
	"time"
)

const (
	maxSessions  = 32
	sessionLimit = 2 * time.Minute
	maxBodyBytes = 512 << 10
)

type server struct {
	coursesDir string
	qemu       string
	origin     string
	builder    *Builder

	mu       sync.Mutex
	sessions map[string]*Session
}

func main() {
	addr := flag.String("addr", ":8080", "listen address")
	courses := flag.String("courses", "../../courses", "courses directory")
	qemu := flag.String("qemu", "../../emulator/qemu/out/bin/qemu-system-arm", "qemu-system-arm with the blackpill-f411 machine")
	work := flag.String("work", os.TempDir()+"/runner-builds", "build directory")
	origin := flag.String("origin", "*", "allowed CORS origin")
	flag.Parse()

	if err := os.MkdirAll(*work, 0o755); err != nil {
		log.Fatal(err)
	}
	s := &server{coursesDir: *courses, qemu: *qemu, origin: *origin, builder: NewBuilder(*work), sessions: map[string]*Session{}}

	mux := http.NewServeMux()
	mux.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("ok")) })
	mux.HandleFunc("GET /api/labs/{course}/{module}", s.getLab)
	mux.HandleFunc("POST /api/labs/{course}/{module}/build", s.build)
	mux.HandleFunc("POST /api/labs/{course}/{module}/check", s.check)
	mux.HandleFunc("GET /api/builds/{id}/{file}", s.download)
	mux.HandleFunc("POST /api/runs", s.startRun)
	mux.HandleFunc("GET /api/runs/{id}/events", s.events)
	mux.HandleFunc("POST /api/runs/{id}/input", s.input)
	mux.HandleFunc("DELETE /api/runs/{id}", s.stopRun)

	log.Printf("runner listening on %s", *addr)
	log.Fatal(http.ListenAndServe(*addr, s.cors(mux)))
}

func (s *server) cors(h http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", s.origin)
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, DELETE")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type")
		if r.Method == http.MethodOptions {
			return
		}
		h.ServeHTTP(w, r)
	})
}

func writeJSON(w http.ResponseWriter, status int, v any) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(v)
}

func writeErr(w http.ResponseWriter, status int, err error) {
	writeJSON(w, status, map[string]string{"error": err.Error()})
}

func (s *server) lab(w http.ResponseWriter, r *http.Request) *Lab {
	lab, err := loadLab(s.coursesDir, r.PathValue("course"), r.PathValue("module"))
	if err != nil {
		writeErr(w, http.StatusNotFound, err)
		return nil
	}
	return lab
}

func (s *server) getLab(w http.ResponseWriter, r *http.Request) {
	lab := s.lab(w, r)
	if lab == nil {
		return
	}
	files, err := lab.Files()
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"lab": lab, "files": files})
}

func (s *server) buildFromRequest(w http.ResponseWriter, r *http.Request) *BuildResult {
	lab := s.lab(w, r)
	if lab == nil {
		return nil
	}
	var req struct {
		Files map[string]string `json:"files"`
	}
	if err := json.NewDecoder(http.MaxBytesReader(w, r.Body, maxBodyBytes)).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, err)
		return nil
	}
	res, err := s.builder.Build(r.Context(), lab, req.Files)
	if err != nil {
		writeErr(w, http.StatusBadRequest, err)
		return nil
	}
	return res
}

func (s *server) build(w http.ResponseWriter, r *http.Request) {
	if res := s.buildFromRequest(w, r); res != nil {
		writeJSON(w, http.StatusOK, res)
	}
}

func (s *server) check(w http.ResponseWriter, r *http.Request) {
	res := s.buildFromRequest(w, r)
	if res == nil {
		return
	}
	if !res.OK {
		writeJSON(w, http.StatusOK, map[string]any{"build": res})
		return
	}
	ctx, cancel := context.WithTimeout(r.Context(), 30*time.Second)
	defer cancel()
	report, err := RunChecks(ctx, s.qemu, res)
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err)
		return
	}
	writeJSON(w, http.StatusOK, map[string]any{"build": res, "report": report})
}

func (s *server) download(w http.ResponseWriter, r *http.Request) {
	b := s.builder.Get(r.PathValue("id"))
	if b == nil || !b.OK {
		writeErr(w, http.StatusNotFound, errors.New("no such build"))
		return
	}
	switch r.PathValue("file") {
	case "kernel.bin":
		w.Header().Set("Content-Type", "application/octet-stream")
		http.ServeFile(w, r, b.Bin())
	case "kernel.elf":
		w.Header().Set("Content-Type", "application/octet-stream")
		http.ServeFile(w, r, b.ELF())
	default:
		writeErr(w, http.StatusNotFound, errors.New("no such file"))
	}
}

func newID() string {
	b := make([]byte, 12)
	rand.Read(b)
	return hex.EncodeToString(b)
}

func (s *server) startRun(w http.ResponseWriter, r *http.Request) {
	var req struct {
		BuildID string `json:"buildId"`
	}
	if err := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096)).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, err)
		return
	}
	b := s.builder.Get(req.BuildID)
	if b == nil || !b.OK {
		writeErr(w, http.StatusNotFound, errors.New("no such build"))
		return
	}
	s.mu.Lock()
	full := len(s.sessions) >= maxSessions
	s.mu.Unlock()
	if full {
		writeErr(w, http.StatusServiceUnavailable, errors.New("all emulators are busy, try again in a minute"))
		return
	}

	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	sess, err := StartQEMU(ctx, s.qemu, b.lab.Board, b.ELF())
	if err != nil {
		writeErr(w, http.StatusInternalServerError, err)
		return
	}
	id := newID()
	s.mu.Lock()
	s.sessions[id] = sess
	s.mu.Unlock()
	go func() {
		select {
		case <-sess.Done():
		case <-time.After(sessionLimit):
			sess.Stop()
		}
		s.mu.Lock()
		delete(s.sessions, id)
		s.mu.Unlock()
	}()
	writeJSON(w, http.StatusOK, map[string]any{"runId": id, "limitMs": sessionLimit.Milliseconds()})
}

func (s *server) session(w http.ResponseWriter, r *http.Request) *Session {
	s.mu.Lock()
	sess := s.sessions[r.PathValue("id")]
	s.mu.Unlock()
	if sess == nil {
		writeErr(w, http.StatusNotFound, errors.New("run has ended"))
	}
	return sess
}

// events streams the session as Server-Sent Events, one JSON event per message.
func (s *server) events(w http.ResponseWriter, r *http.Request) {
	sess := s.session(w, r)
	if sess == nil {
		return
	}
	fl, ok := w.(http.Flusher)
	if !ok {
		writeErr(w, http.StatusInternalServerError, errors.New("streaming unsupported"))
		return
	}
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("X-Accel-Buffering", "no")
	fl.Flush()
	for {
		select {
		case e, ok := <-sess.Events:
			if !ok {
				return
			}
			data, _ := json.Marshal(e)
			fmt.Fprintf(w, "data: %s\n\n", data)
			fl.Flush()
		case <-r.Context().Done():
			sess.Stop()
			return
		}
	}
}

func (s *server) input(w http.ResponseWriter, r *http.Request) {
	sess := s.session(w, r)
	if sess == nil {
		return
	}
	var req struct {
		Pin  string `json:"pin"`
		V    int    `json:"v"`
		UART string `json:"uart"`
	}
	if err := json.NewDecoder(http.MaxBytesReader(w, r.Body, 4096)).Decode(&req); err != nil {
		writeErr(w, http.StatusBadRequest, err)
		return
	}
	var err error
	if req.Pin != "" {
		err = sess.SetPin(req.Pin, req.V)
	} else {
		err = sess.SendUART(req.UART)
	}
	if err != nil {
		writeErr(w, http.StatusBadRequest, err)
		return
	}
	w.WriteHeader(http.StatusNoContent)
}

func (s *server) stopRun(w http.ResponseWriter, r *http.Request) {
	if sess := s.session(w, r); sess != nil {
		sess.Stop()
		w.WriteHeader(http.StatusNoContent)
	}
}
