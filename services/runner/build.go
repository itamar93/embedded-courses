package main

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"sort"
	"sync"
	"time"
)

const (
	buildTimeout   = 20 * time.Second
	maxFileBytes   = 64 << 10
	maxCachedBuild = 256
)

type BuildResult struct {
	ID   string `json:"id"`
	OK   bool   `json:"ok"`
	Log  string `json:"log"`
	Size string `json:"size,omitempty"`

	dir string
	lab *Lab
}

func (b *BuildResult) ELF() string { return filepath.Join(b.dir, "build", "kernel.elf") }
func (b *BuildResult) Bin() string { return filepath.Join(b.dir, "build", "kernel.bin") }

// Builder compiles labs with arm-none-eabi-gcc and caches results by content hash.
type Builder struct {
	workDir string

	mu     sync.Mutex
	cache  map[string]*BuildResult
	order  []string
	active map[string]*sync.Mutex
}

func NewBuilder(workDir string) *Builder {
	return &Builder{workDir: workDir, cache: map[string]*BuildResult{}, active: map[string]*sync.Mutex{}}
}

func (b *Builder) Get(id string) *BuildResult {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.cache[id]
}

func buildID(lab *Lab, files map[string]string) string {
	keys := make([]string, 0, len(files))
	for k := range files {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	h := sha256.New()
	fmt.Fprintf(h, "%s/%s\n", lab.Course, lab.Module)
	for _, k := range keys {
		json.NewEncoder(h).Encode([]string{k, files[k]})
	}
	return hex.EncodeToString(h.Sum(nil))[:24]
}

// Build copies the starter lab, overlays the learner's editable files and runs make.
// Files outside the lab's editable list are rejected.
func (b *Builder) Build(ctx context.Context, lab *Lab, files map[string]string) (*BuildResult, error) {
	for path, content := range files {
		if !lab.isEditable(path) {
			return nil, fmt.Errorf("%s is not editable in this lab", path)
		}
		if len(content) > maxFileBytes {
			return nil, fmt.Errorf("%s is larger than %d KB", path, maxFileBytes>>10)
		}
	}
	id := buildID(lab, files)

	b.mu.Lock()
	if r, ok := b.cache[id]; ok {
		b.mu.Unlock()
		return r, nil
	}
	lock, ok := b.active[id]
	if !ok {
		lock = &sync.Mutex{}
		b.active[id] = lock
	}
	b.mu.Unlock()

	lock.Lock()
	defer lock.Unlock()
	if r := b.Get(id); r != nil {
		return r, nil
	}

	dir := filepath.Join(b.workDir, id)
	os.RemoveAll(dir)
	if err := os.CopyFS(dir, os.DirFS(lab.dir)); err != nil {
		return nil, err
	}
	os.RemoveAll(filepath.Join(dir, "build"))
	for path, content := range files {
		if err := os.WriteFile(filepath.Join(dir, filepath.FromSlash(path)), []byte(content), 0o644); err != nil {
			return nil, err
		}
	}

	cctx, cancel := context.WithTimeout(ctx, buildTimeout)
	defer cancel()
	cmd := exec.CommandContext(cctx, "make", "-s", "APP="+lab.App, "all")
	cmd.Dir = dir
	out, err := cmd.CombinedOutput()

	r := &BuildResult{ID: id, OK: err == nil, Log: string(out), dir: dir, lab: lab}
	if cctx.Err() == context.DeadlineExceeded {
		r.Log += "\nbuild timed out"
	}
	if r.OK {
		r.Size = string(out)
		r.Log = ""
	}

	b.mu.Lock()
	b.cache[id] = r
	b.order = append(b.order, id)
	delete(b.active, id)
	if len(b.order) > maxCachedBuild {
		old := b.order[0]
		b.order = b.order[1:]
		if o := b.cache[old]; o != nil {
			os.RemoveAll(o.dir)
		}
		delete(b.cache, old)
	}
	b.mu.Unlock()
	return r, nil
}
