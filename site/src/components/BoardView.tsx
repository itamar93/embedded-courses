interface Props {
  pins: Record<string, number>;
  running: boolean;
  onKey: (pressed: boolean) => void;
}

/** A WeAct Black Pill seen from above: the PC13 LED and the KEY button (PA0) are live. */
export default function BoardView({ pins, running, onKey }: Props) {
  // PC13 drives the LED's cathode: the LED is lit when the pin is low
  const ledOn = running && pins.PC13 === 0;
  const header = Array.from({ length: 20 }, (_, i) => i);

  return (
    <svg viewBox="0 0 520 200" className="w-full select-none" role="img" aria-label="Black Pill board">
      <defs>
        <radialGradient id="led-glow">
          <stop offset="0%" stopColor="#7dffb2" stopOpacity="0.9" />
          <stop offset="100%" stopColor="#3ddc97" stopOpacity="0" />
        </radialGradient>
      </defs>
      <rect x="10" y="30" width="500" height="140" rx="8" fill="#1d2b5a" stroke="#2f4380" />
      {header.map((i) => (
        <g key={i}>
          <rect x={52 + i * 21} y="38" width="12" height="12" rx="2" fill="#c9a24a" opacity="0.85" />
          <rect x={52 + i * 21} y="150" width="12" height="12" rx="2" fill="#c9a24a" opacity="0.85" />
        </g>
      ))}
      {/* USB-C */}
      <rect x="0" y="82" width="38" height="36" rx="5" fill="#9aa3b5" stroke="#cfd6e3" />
      {/* MCU */}
      <rect x="220" y="68" width="64" height="64" rx="3" fill="#121722" stroke="#30384a" />
      <text x="252" y="96" textAnchor="middle" fontSize="8" fill="#8892a6" fontFamily="monospace">STM32</text>
      <text x="252" y="108" textAnchor="middle" fontSize="8" fill="#8892a6" fontFamily="monospace">F411CE</text>
      {/* PC13 LED */}
      {ledOn && <circle cx="120" cy="78" r="26" fill="url(#led-glow)" />}
      <rect x="112" y="72" width="16" height="12" rx="2" fill={ledOn ? '#7dffb2' : '#3a4a3f'} stroke="#0e1a14" />
      <text x="120" y="98" textAnchor="middle" fontSize="9" fill="#c8d1e6" fontFamily="monospace">PC13</text>
      {/* Buttons */}
      <g>
        <rect x="90" y="112" width="28" height="22" rx="3" fill="#2a2f3a" />
        <circle cx="104" cy="123" r="6" fill="#555c6b" />
        <text x="104" y="146" textAnchor="middle" fontSize="8" fill="#8892a6" fontFamily="monospace">BOOT0</text>
      </g>
      <g
        className={running ? 'cursor-pointer' : 'cursor-not-allowed'}
        onPointerDown={() => running && onKey(true)}
        onPointerUp={() => running && onKey(false)}
        onPointerLeave={() => running && onKey(false)}
      >
        <rect x="380" y="112" width="28" height="22" rx="3" fill="#2a2f3a" />
        <circle cx="394" cy="123" r="6" fill={pins.PA0 === 0 && running ? '#3ddc97' : '#7b8496'} />
        <text x="394" y="146" textAnchor="middle" fontSize="8" fill="#c8d1e6" fontFamily="monospace">KEY · PA0</text>
      </g>
      <text x="470" y="105" textAnchor="middle" fontSize="9" fill="#8892a6" fontFamily="monospace">SWD</text>
    </svg>
  );
}
