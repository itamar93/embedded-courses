package main

import (
	"fmt"
	"io/fs"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"

	"gopkg.in/yaml.v3"
)

// Lab is a lab.yaml file plus where its starter project lives.
type Lab struct {
	Course   string   `json:"course"`
	Module   string   `json:"module"`
	Board    string   `json:"board" yaml:"board"`
	Modes    []string `json:"modes" yaml:"modes"`
	App      string   `json:"app" yaml:"app"`
	Editable []string `json:"editable" yaml:"editable"`
	RunMs    int      `json:"runMs" yaml:"run_ms"`
	Checks   []Check  `json:"checks" yaml:"checks"`

	dir string
}

type Check struct {
	Name   string      `json:"name" yaml:"name"`
	Hint   string      `json:"hint,omitempty" yaml:"hint"`
	Expect CheckExpect `json:"-" yaml:"expect"`
}

type CheckExpect struct {
	Reg     string `yaml:"reg"`
	Mask    uint32 `yaml:"mask"`
	Equals  uint32 `yaml:"equals"`
	Pin     string `yaml:"pin"`
	Toggles *struct {
		Min int `yaml:"min"`
		Max int `yaml:"max"`
	} `yaml:"toggles"`
	UartContains string `yaml:"uart_contains"`
}

type LabFile struct {
	Path     string `json:"path"`
	Content  string `json:"content"`
	Editable bool   `json:"editable"`
}

var idPattern = regexp.MustCompile(`^[a-z0-9][a-z0-9-]*$`)

func loadLab(coursesDir, course, module string) (*Lab, error) {
	if !idPattern.MatchString(course) || !idPattern.MatchString(module) {
		return nil, fmt.Errorf("bad lab id")
	}
	base := filepath.Join(coursesDir, course, module)
	raw, err := os.ReadFile(filepath.Join(base, "lab.yaml"))
	if err != nil {
		return nil, fmt.Errorf("no such lab %s/%s", course, module)
	}
	lab := &Lab{Course: course, Module: module, dir: filepath.Join(base, "lab")}
	if err := yaml.Unmarshal(raw, lab); err != nil {
		return nil, fmt.Errorf("lab.yaml: %w", err)
	}
	if lab.RunMs <= 0 {
		lab.RunMs = 3000
	}
	return lab, nil
}

func (l *Lab) isEditable(path string) bool {
	for _, e := range l.Editable {
		if e == path {
			return true
		}
	}
	return false
}

// Files returns the starter project, editable files first.
func (l *Lab) Files() ([]LabFile, error) {
	var files []LabFile
	err := filepath.WalkDir(l.dir, func(p string, d fs.DirEntry, err error) error {
		if err != nil {
			return err
		}
		if d.IsDir() {
			if d.Name() == "build" {
				return filepath.SkipDir
			}
			return nil
		}
		rel, _ := filepath.Rel(l.dir, p)
		rel = filepath.ToSlash(rel)
		content, err := os.ReadFile(p)
		if err != nil {
			return err
		}
		files = append(files, LabFile{Path: rel, Content: string(content), Editable: l.isEditable(rel)})
		return nil
	})
	sort.SliceStable(files, func(i, j int) bool {
		if files[i].Editable != files[j].Editable {
			return files[i].Editable
		}
		return strings.Compare(files[i].Path, files[j].Path) < 0
	})
	return files, err
}
