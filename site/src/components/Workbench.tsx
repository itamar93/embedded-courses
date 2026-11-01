import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import Editor from '@monaco-editor/react';
import BoardView from './BoardView';
import SerialConsole, { type ConsoleHandle } from './SerialConsole';
import { track } from '../lib/analytics';
import '../lib/monaco';

interface LabFile {
  path: string;
  content: string;
  editable: boolean;
}

interface Build {
  id: string;
  ok: boolean;
  log: string;
  size?: string;
}

interface CheckResult {
  name: string;
  passed: boolean;
  detail?: string;
  hint?: string;
}

interface Props {
  runnerUrl: string;
  course: string;
  module: string;
}

type Status = 'loading' | 'idle' | 'building' | 'running' | 'checking' | 'error';

const draftKey = (course: string, module: string) => `draft:${course}/${module}`;

function loadDrafts(course: string, module: string): Record<string, string> {
  try {
    return JSON.parse(localStorage.getItem(draftKey(course, module)) ?? '{}');
  } catch {
    return {};
  }
}

function saveDrafts(course: string, module: string, drafts: Record<string, string>) {
  try {
    localStorage.setItem(draftKey(course, module), JSON.stringify(drafts));
  } catch {
    // Private windows can refuse storage; drafts then live only in memory
  }
}

export default function Workbench({ runnerUrl, course, module }: Props) {
  const api = `${runnerUrl}/api`;
  const [files, setFiles] = useState<LabFile[]>([]);
  const [drafts, setDrafts] = useState<Record<string, string>>({});
  const [active, setActive] = useState<string>('');
  const [status, setStatus] = useState<Status>('loading');
  const [error, setError] = useState('');
  const [pins, setPins] = useState<Record<string, number>>({});
  const [build, setBuild] = useState<Build | null>(null);
  const [results, setResults] = useState<CheckResult[] | null>(null);
  const runRef = useRef<{ id: string; source: EventSource } | null>(null);
  const consoleRef = useRef<ConsoleHandle>(null);

  useEffect(() => {
    fetch(`${api}/labs/${course}/${module}`)
      .then((r) => (r.ok ? r.json() : Promise.reject(new Error(`runner answered ${r.status}`))))
      .then((data: { files: LabFile[] }) => {
        setFiles(data.files);
        setDrafts(loadDrafts(course, module));
        setActive(data.files.find((f) => f.editable)?.path ?? data.files[0]?.path ?? '');
        setStatus('idle');
      })
      .catch((e) => {
        setError(`Could not reach the lab service (${e.message}). Is the runner running at ${runnerUrl}?`);
        setStatus('error');
      });
    return () => stopRun();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [api, course, module]);

  const activeFile = files.find((f) => f.path === active);
  const editedFiles = useMemo(() => {
    const out: Record<string, string> = {};
    for (const f of files) if (f.editable) out[f.path] = drafts[f.path] ?? f.content;
    return out;
  }, [files, drafts]);

  const onEdit = (value: string | undefined) => {
    if (!activeFile?.editable) return;
    const next = { ...drafts, [activeFile.path]: value ?? '' };
    setDrafts(next);
    saveDrafts(course, module, next);
  };

  const resetFile = () => {
    if (!activeFile?.editable) return;
    const { [activeFile.path]: _, ...rest } = drafts;
    setDrafts(rest);
    saveDrafts(course, module, rest);
  };

  const stopRun = useCallback(() => {
    const run = runRef.current;
    if (!run) return;
    run.source.close();
    fetch(`${api}/runs/${run.id}`, { method: 'DELETE' }).catch(() => {});
    runRef.current = null;
    setStatus((s) => (s === 'running' ? 'idle' : s));
  }, [api]);

  const compile = async (): Promise<Build | null> => {
    setStatus('building');
    setResults(null);
    const res = await fetch(`${api}/labs/${course}/${module}/build`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ files: editedFiles }),
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error ?? 'build failed');
    setBuild(data);
    if (!data.ok) {
      consoleRef.current?.write(`\x1b[31m${data.log}\x1b[0m\r\n`);
      setStatus('idle');
      track('lab_build_failed', { course, module });
      return null;
    }
    return data;
  };

  const run = async () => {
    stopRun();
    consoleRef.current?.clear();
    setPins({});
    try {
      const b = await compile();
      if (!b) return;
      const res = await fetch(`${api}/runs`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ buildId: b.id }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error);
      const source = new EventSource(`${api}/runs/${data.runId}/events`);
      runRef.current = { id: data.runId, source };
      setStatus('running');
      track('lab_run', { course, module });
      source.onmessage = (msg) => {
        const e = JSON.parse(msg.data);
        if (e.type === 'uart') consoleRef.current?.write(e.data);
        else if (e.type === 'pin') setPins((p) => ({ ...p, [e.pin]: e.v }));
        else if (e.type === 'exit') {
          consoleRef.current?.write('\r\n\x1b[90m[emulator stopped]\x1b[0m\r\n');
          stopRun();
        }
      };
      source.onerror = () => stopRun();
    } catch (e) {
      setError((e as Error).message);
      setStatus('idle');
    }
  };

  const check = async () => {
    stopRun();
    consoleRef.current?.clear();
    setStatus('checking');
    setResults(null);
    try {
      const res = await fetch(`${api}/labs/${course}/${module}/check`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ files: editedFiles }),
      });
      const data = await res.json();
      if (!res.ok) throw new Error(data.error);
      setBuild(data.build);
      if (!data.build.ok) {
        consoleRef.current?.write(`\x1b[31m${data.build.log}\x1b[0m\r\n`);
      } else {
        consoleRef.current?.write(data.report.uart);
        setResults(data.report.results);
        track(data.report.passed ? 'lab_passed' : 'lab_check_failed', { course, module });
      }
    } catch (e) {
      setError((e as Error).message);
    }
    setStatus('idle');
  };

  const sendKey = (pressed: boolean) => {
    const id = runRef.current?.id;
    if (!id) return;
    // KEY pulls PA0 to ground when pressed
    setPins((p) => ({ ...p, PA0: pressed ? 0 : 1 }));
    fetch(`${api}/runs/${id}/input`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ pin: 'PA0', v: pressed ? 0 : 1 }),
    }).catch(() => {});
  };

  const sendSerial = (data: string) => {
    const id = runRef.current?.id;
    if (!id) return;
    fetch(`${api}/runs/${id}/input`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ uart: data }),
    }).catch(() => {});
  };

  if (status === 'error') {
    return <div className="rounded-lg border border-red-500/40 bg-red-500/10 p-4 text-sm text-red-200">{error}</div>;
  }

  const busy = status === 'building' || status === 'checking' || status === 'loading';
  const passed = results?.every((r) => r.passed);

  return (
    <div className="flex h-full min-h-[760px] flex-col overflow-hidden rounded-lg border border-line bg-panel">
      <div className="flex flex-wrap items-center gap-2 border-b border-line p-2">
        <button onClick={run} disabled={busy} className="rounded bg-accent px-3 py-1.5 text-sm font-semibold text-ink disabled:opacity-50">
          {status === 'building' ? 'Compiling…' : status === 'running' ? 'Restart' : '▶ Run'}
        </button>
        {status === 'running' && (
          <button onClick={stopRun} className="rounded border border-line px-3 py-1.5 text-sm text-white">■ Stop</button>
        )}
        <button onClick={check} disabled={busy} className="rounded border border-accent-dim px-3 py-1.5 text-sm text-accent disabled:opacity-50">
          {status === 'checking' ? 'Checking…' : '✓ Check'}
        </button>
        <div className="flex-1" />
        {build?.ok && (
          <a href={`${api}/builds/${build.id}/kernel.bin`} download="kernel.bin" className="text-xs text-slate-300 hover:text-white">
            Download .bin
          </a>
        )}
        <button disabled title="Flashing over WebUSB is coming next" className="rounded border border-line px-3 py-1.5 text-xs text-slate-500">
          Flash board
        </button>
      </div>

      <div className="flex gap-1 overflow-x-auto border-b border-line px-2 pt-2">
        {files.map((f) => (
          <button
            key={f.path}
            onClick={() => setActive(f.path)}
            className={`whitespace-nowrap rounded-t px-2.5 py-1 font-mono text-xs ${
              f.path === active ? 'bg-ink text-white' : 'text-slate-400 hover:text-white'
            }`}
          >
            {f.editable ? '✎ ' : ''}
            {f.path}
            {drafts[f.path] !== undefined && f.editable ? ' •' : ''}
          </button>
        ))}
      </div>

      <div className="relative min-h-[300px] flex-1">
        {activeFile && (
          <Editor
            height="100%"
            theme="vs-dark"
            language={activeFile.path.endsWith('Makefile') ? 'makefile' : 'c'}
            path={activeFile.path}
            value={drafts[activeFile.path] ?? activeFile.content}
            onChange={onEdit}
            options={{ readOnly: !activeFile.editable, minimap: { enabled: false }, fontSize: 13, scrollBeyondLastLine: false }}
          />
        )}
        {activeFile?.editable && drafts[activeFile.path] !== undefined && (
          <button onClick={resetFile} className="absolute right-3 top-2 rounded bg-ink/80 px-2 py-1 text-xs text-slate-300 hover:text-white">
            Reset file
          </button>
        )}
      </div>

      <div className="grid shrink-0 border-t border-line md:h-56 md:grid-cols-2">
        <div className="border-b border-line p-2 md:border-b-0 md:border-r">
          <p className="mb-1 font-mono text-[11px] uppercase tracking-wider text-slate-500">
            Board {status === 'running' && <span className="text-accent">● running</span>}
          </p>
          <BoardView pins={pins} running={status === 'running'} onKey={sendKey} />
        </div>
        <div className="flex min-h-0 flex-col p-2">
          <p className="mb-1 font-mono text-[11px] uppercase tracking-wider text-slate-500">Serial · USART2 · 9600</p>
          <div className="h-40 min-h-0 flex-1 overflow-hidden rounded bg-ink p-1 md:h-auto">
            <SerialConsole ref={consoleRef} onInput={sendSerial} />
          </div>
        </div>
      </div>

      {(results || error) && (
        <div className="max-h-56 shrink-0 overflow-y-auto border-t border-line p-3 text-sm">
          {error && <p className="mb-2 text-red-300">{error}</p>}
          {results && (
            <>
              <p className={`mb-2 font-semibold ${passed ? 'text-accent' : 'text-warn'}`}>
                {passed ? 'All checks passed. Nice work!' : `${results.filter((r) => r.passed).length} of ${results.length} checks passed`}
              </p>
              <ul className="space-y-1.5">
                {results.map((r) => (
                  <li key={r.name}>
                    <span className={r.passed ? 'text-accent' : 'text-red-300'}>{r.passed ? '✓' : '✗'}</span>{' '}
                    <span className="text-white">{r.name}</span>
                    {r.detail && <span className="ml-2 font-mono text-xs text-slate-400">{r.detail}</span>}
                    {!r.passed && r.hint && <p className="ml-5 text-xs text-slate-400">Hint: {r.hint}</p>}
                  </li>
                ))}
              </ul>
            </>
          )}
        </div>
      )}
    </div>
  );
}
