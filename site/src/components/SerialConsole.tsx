import { useEffect, useImperativeHandle, useRef, type Ref } from 'react';
import { Terminal } from '@xterm/xterm';
import { FitAddon } from '@xterm/addon-fit';
import '@xterm/xterm/css/xterm.css';

export interface ConsoleHandle {
  write: (text: string) => void;
  clear: () => void;
}

interface Props {
  ref: Ref<ConsoleHandle>;
  onInput?: (data: string) => void;
}

export default function SerialConsole({ ref, onInput }: Props) {
  const el = useRef<HTMLDivElement>(null);
  const term = useRef<Terminal | null>(null);
  const inputRef = useRef(onInput);
  inputRef.current = onInput;

  useEffect(() => {
    const t = new Terminal({
      convertEol: true,
      fontFamily: 'JetBrains Mono, monospace',
      fontSize: 13,
      theme: { background: '#0b1220', foreground: '#c8f7dc', cursor: '#3ddc97' },
    });
    const fit = new FitAddon();
    t.loadAddon(fit);
    t.open(el.current!);
    fit.fit();
    t.onData((d) => inputRef.current?.(d));
    const ro = new ResizeObserver(() => fit.fit());
    ro.observe(el.current!);
    term.current = t;
    return () => {
      ro.disconnect();
      t.dispose();
    };
  }, []);

  useImperativeHandle(ref, () => ({
    write: (text) => term.current?.write(text),
    clear: () => term.current?.reset(),
  }));

  return <div ref={el} className="h-full w-full" />;
}
