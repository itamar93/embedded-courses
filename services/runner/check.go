package main

import (
	"context"
	"fmt"
	"strings"
	"time"
)

// Registers the checks in lab.yaml can name.
var registers = map[string]uint32{
	"RCC_AHB1ENR":  0x40023830,
	"RCC_APB1ENR":  0x40023840,
	"RCC_APB2ENR":  0x40023844,
	"USART2_SR":    0x40004400,
	"USART2_BRR":   0x40004408,
	"USART2_CR1":   0x4000440c,
	"SYSTICK_CTRL": 0xe000e010,
	"SYSTICK_LOAD": 0xe000e014,
}

func init() {
	ports := map[string]uint32{"A": 0x40020000, "B": 0x40020400, "C": 0x40020800, "D": 0x40020c00, "E": 0x40021000, "H": 0x40021c00}
	regs := map[string]uint32{"MODER": 0x00, "OTYPER": 0x04, "OSPEEDR": 0x08, "PUPDR": 0x0c, "IDR": 0x10, "ODR": 0x14, "AFRL": 0x20, "AFRH": 0x24}
	for p, base := range ports {
		for r, off := range regs {
			registers["GPIO"+p+"_"+r] = base + off
		}
	}
}

type CheckResult struct {
	Name   string `json:"name"`
	Passed bool   `json:"passed"`
	Detail string `json:"detail,omitempty"`
	Hint   string `json:"hint,omitempty"`
}

type CheckReport struct {
	Passed  bool          `json:"passed"`
	Results []CheckResult `json:"results"`
	UART    string        `json:"uart"`
}

// RunChecks boots the build headless for lab.RunMs, then grades it against lab.yaml.
func RunChecks(ctx context.Context, qemu string, b *BuildResult) (*CheckReport, error) {
	lab := b.lab
	s, err := StartQEMU(ctx, qemu, lab.Board, b.ELF())
	if err != nil {
		return nil, err
	}
	defer s.Stop()

	var uart strings.Builder
	pinEvents := map[string]int{}
	deadline := time.After(time.Duration(lab.RunMs) * time.Millisecond)
collect:
	for {
		select {
		case e, ok := <-s.Events:
			if !ok {
				break collect
			}
			switch e.Type {
			case "uart":
				uart.WriteString(e.Data)
			case "pin":
				pinEvents[e.Pin]++
			}
		case <-deadline:
			break collect
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
	s.Pause()

	report := &CheckReport{Passed: true, UART: uart.String()}
	for _, c := range lab.Checks {
		r := CheckResult{Name: c.Name}
		x := c.Expect
		switch {
		case x.Reg != "":
			addr, ok := registers[x.Reg]
			if !ok {
				r.Detail = "unknown register " + x.Reg
				break
			}
			v, err := s.ReadWord(addr)
			if err != nil {
				r.Detail = err.Error()
				break
			}
			r.Passed = v&x.Mask == x.Equals
			r.Detail = fmt.Sprintf("%s = 0x%08X", x.Reg, v)
		case x.Pin != "" && x.Toggles != nil:
			// The first event is the pin becoming a driven output
			toggles := max(pinEvents[x.Pin]-1, 0)
			r.Passed = toggles >= x.Toggles.Min && toggles <= x.Toggles.Max
			r.Detail = fmt.Sprintf("%s toggled %d times in %d ms (expected %d to %d)",
				x.Pin, toggles, lab.RunMs, x.Toggles.Min, x.Toggles.Max)
		case x.UartContains != "":
			r.Passed = strings.Contains(report.UART, x.UartContains)
			if !r.Passed {
				r.Detail = fmt.Sprintf("serial output never contained %q", x.UartContains)
			}
		default:
			r.Detail = "check has no expectation"
		}
		if !r.Passed {
			r.Hint = c.Hint
			report.Passed = false
		}
		report.Results = append(report.Results, r)
	}
	return report, nil
}
