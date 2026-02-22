#!/usr/bin/env python3
"""
seq_editor.py  –  Piano-roll step-sequence editor
Generates a `static const SequenceStep s_sequence[]` C array for sequencer.h

Usage:
    python3 tools/seq_editor.py
"""

import tkinter as tk
from tkinter import ttk, messagebox
import math
import struct
import wave
import io
import threading
import subprocess
import time

# ---------------------------------------------------------------------------
# Music constants
# ---------------------------------------------------------------------------
OCTAVES       = 4        # C2..B5
NOTES_PER_OCT = 12
TOTAL_NOTES   = OCTAVES * NOTES_PER_OCT   # 48 rows
BASE_MIDI     = 36       # C2

NOTE_NAMES = ['C','C#','D','D#','E','F','F#','G','G#','A','A#','B']
BLACK_NOTES = {1, 3, 6, 8, 10}   # indices within octave that are black keys

MAX_STEPS   = 32

CELL_W = 36   # px per step column
CELL_H = 14   # px per note row

ACTIVE_COLOR   = '#4FC3F7'
REST_COLOR     = '#444444'
GRID_BG        = '#1E1E1E'
GRID_LINE      = '#333333'
BLACK_KEY_BG   = '#2A2A2A'
WHITE_KEY_BG   = '#303030'
HEADER_BG      = '#252525'
LABEL_FG       = '#CCCCCC'


def midi_to_hz(note: int) -> float:
    return 440.0 * math.pow(2.0, (note - 69) / 12.0)


def note_label(midi: int) -> str:
    return f"{NOTE_NAMES[midi % 12]}{midi // 12 - 1}"


# ---------------------------------------------------------------------------
# Audio synthesis – pure stdlib, output via aplay
# ---------------------------------------------------------------------------
SAMPLERATE = 44100

def _sine_wave(freq: float, duration_s: float, amplitude: float = 0.4,
               samplerate: int = SAMPLERATE) -> bytes:
    """Generate signed 16-bit mono sine wave PCM bytes."""
    n = int(samplerate * duration_s)
    samples = []
    for i in range(n):
        # simple sine with fast linear attack/release (5 ms) to avoid clicks
        t = i / samplerate
        env = 1.0
        attack = min(0.005 * samplerate, n * 0.1)
        release_start = n - min(0.005 * samplerate, n * 0.1)
        if i < attack:
            env = i / attack
        elif i > release_start:
            env = (n - i) / (n - release_start)
        v = amplitude * env * math.sin(2.0 * math.pi * freq * t)
        samples.append(max(-32768, min(32767, int(v * 32767))))
    return struct.pack(f'<{n}h', *samples)


def _silence(duration_s: float, samplerate: int = SAMPLERATE) -> bytes:
    n = int(samplerate * duration_s)
    return b'\x00\x00' * n


def _build_wav(pcm_bytes: bytes, samplerate: int = SAMPLERATE) -> bytes:
    """Wrap raw signed-16 mono PCM in a WAV container (in memory)."""
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(samplerate)
        wf.writeframes(pcm_bytes)
    return buf.getvalue()


class AudioPlayer:
    """Renders the sequence to WAV and streams it through aplay."""

    def __init__(self):
        self._proc   = None
        self._thread = None
        self._stop   = threading.Event()

    def play(self, steps_data: list):
        """
        steps_data: list of (midi_note, velocity, gate_ms, step_ms)
        velocity=0 or note=0  →  rest
        """
        self.stop()
        self._stop.clear()
        self._thread = threading.Thread(target=self._run,
                                        args=(steps_data,), daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._proc and self._proc.poll() is None:
            self._proc.terminate()
        if self._thread:
            self._thread.join(timeout=1.0)
        self._proc   = None
        self._thread = None

    def is_playing(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def _run(self, steps_data):
        # Build complete PCM for the sequence
        pcm = b''
        for (note, velocity, gate_ms, step_ms) in steps_data:
            if self._stop.is_set():
                return
            gate_s = gate_ms / 1000.0
            step_s = step_ms / 1000.0
            gap_s  = max(0.0, step_s - gate_s)
            if note > 0 and velocity > 0:
                amp = (velocity / 127.0) * 0.45
                pcm += _sine_wave(midi_to_hz(note), gate_s, amplitude=amp)
            else:
                pcm += _silence(gate_s)
            pcm += _silence(gap_s)

        wav = _build_wav(pcm)
        if self._stop.is_set():
            return
        try:
            self._proc = subprocess.Popen(
                ['aplay', '-q', '-'],
                stdin=subprocess.PIPE,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            self._proc.stdin.write(wav)
            self._proc.stdin.close()
            self._proc.wait()
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Main application
# ---------------------------------------------------------------------------
class SeqEditor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Step Sequence Editor  –  SequenceStep C Generator")
        self.resizable(True, True)
        self.configure(bg='#1A1A1A')

        # --- state ---
        self.num_steps = tk.IntVar(value=8)
        self.default_velocity = tk.IntVar(value=80)
        self.default_gate_ms  = tk.IntVar(value=180)
        self.default_step_ms  = tk.IntVar(value=250)
        self.loop_var         = tk.BooleanVar(value=True)
        self.array_name_var   = tk.StringVar(value="s_sequence")

        # audio
        self._player = AudioPlayer()

        # grid[step][row]  –  True = active note
        self.grid_active = [[False] * TOTAL_NOTES for _ in range(MAX_STEPS)]
        # per-step overrides (velocity, gate_ms, step_ms); None = use default
        self.step_velocity = [None] * MAX_STEPS
        self.step_gate_ms  = [None] * MAX_STEPS
        self.step_step_ms  = [None] * MAX_STEPS

        self._build_ui()
        self._draw_all()

    # -----------------------------------------------------------------------
    def _build_ui(self):
        # Top toolbar
        toolbar = tk.Frame(self, bg='#252525', pady=4)
        toolbar.pack(side=tk.TOP, fill=tk.X)

        def lbl(parent, text):
            return tk.Label(parent, text=text, bg='#252525', fg=LABEL_FG,
                            font=('Helvetica', 9))

        lbl(toolbar, " Steps:").pack(side=tk.LEFT)
        steps_spin = tk.Spinbox(toolbar, from_=1, to=MAX_STEPS,
                                textvariable=self.num_steps, width=4,
                                bg='#333', fg='white', insertbackground='white',
                                command=self._draw_all)
        steps_spin.pack(side=tk.LEFT, padx=2)

        lbl(toolbar, "  Velocity:").pack(side=tk.LEFT)
        tk.Spinbox(toolbar, from_=1, to=127, textvariable=self.default_velocity,
                   width=4, bg='#333', fg='white').pack(side=tk.LEFT, padx=2)

        lbl(toolbar, "  Gate ms:").pack(side=tk.LEFT)
        tk.Spinbox(toolbar, from_=10, to=9999, textvariable=self.default_gate_ms,
                   width=6, bg='#333', fg='white').pack(side=tk.LEFT, padx=2)

        lbl(toolbar, "  Step ms:").pack(side=tk.LEFT)
        tk.Spinbox(toolbar, from_=10, to=9999, textvariable=self.default_step_ms,
                   width=6, bg='#333', fg='white').pack(side=tk.LEFT, padx=2)

        lbl(toolbar, "  Loop:").pack(side=tk.LEFT)
        tk.Checkbutton(toolbar, variable=self.loop_var, bg='#252525',
                       fg=LABEL_FG, selectcolor='#444').pack(side=tk.LEFT)

        lbl(toolbar, "  Array name:").pack(side=tk.LEFT)
        tk.Entry(toolbar, textvariable=self.array_name_var, width=14,
                 bg='#333', fg='white', insertbackground='white').pack(side=tk.LEFT, padx=2)

        tk.Button(toolbar, text="Generate C", bg='#0D7377', fg='white',
                  relief='flat', padx=8,
                  command=self._show_output).pack(side=tk.RIGHT, padx=6)
        tk.Button(toolbar, text="Clear", bg='#444', fg='white',
                  relief='flat', padx=8,
                  command=self._clear).pack(side=tk.RIGHT, padx=2)

        # Play / Stop buttons
        self._play_btn = tk.Button(toolbar, text="▶  Play", bg='#2E7D32', fg='white',
                                   relief='flat', padx=10,
                                   command=self._play_preview)
        self._play_btn.pack(side=tk.RIGHT, padx=2)
        tk.Button(toolbar, text="■  Stop", bg='#7B1FA2', fg='white',
                  relief='flat', padx=10,
                  command=self._stop_preview).pack(side=tk.RIGHT, padx=2)

        self.protocol("WM_DELETE_WINDOW", self._on_close)

        # ---- scrollable canvas area ----
        piano_width = 52   # key labels
        canvas_frame = tk.Frame(self, bg=GRID_BG)
        canvas_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

        h_scroll = tk.Scrollbar(canvas_frame, orient=tk.HORIZONTAL)
        v_scroll = tk.Scrollbar(canvas_frame, orient=tk.VERTICAL)
        h_scroll.pack(side=tk.BOTTOM, fill=tk.X)
        v_scroll.pack(side=tk.RIGHT,  fill=tk.Y)

        total_w = piano_width + MAX_STEPS * CELL_W + 2
        total_h = TOTAL_NOTES * CELL_H + 20   # +20 for step header

        self.canvas = tk.Canvas(canvas_frame,
                                bg=GRID_BG,
                                scrollregion=(0, 0, total_w, total_h),
                                xscrollcommand=h_scroll.set,
                                yscrollcommand=v_scroll.set,
                                highlightthickness=0)
        self.canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        h_scroll.config(command=self.canvas.xview)
        v_scroll.config(command=self.canvas.yview)

        self.piano_w = piano_width

        self.canvas.bind("<Button-1>",         self._on_click)
        self.canvas.bind("<B1-Motion>",        self._on_drag)
        self.canvas.bind("<Button-3>",         self._on_right_click)
        self.canvas.bind("<MouseWheel>",       self._on_mousewheel)
        self.canvas.bind("<Shift-MouseWheel>", self._on_shift_mousewheel)
        self._drag_state = None   # 'set' or 'clear'

        # Bottom status
        self.status_var = tk.StringVar(value="Click cells to toggle notes  |  Right-click a step header for per-step settings")
        tk.Label(self, textvariable=self.status_var, bg='#181818',
                 fg='#888', font=('Helvetica', 8), anchor='w').pack(
                     side=tk.BOTTOM, fill=tk.X, padx=4)

    # -----------------------------------------------------------------------
    # Drawing
    # -----------------------------------------------------------------------
    def _draw_all(self, *_):
        self.canvas.delete('all')
        steps = self.num_steps.get()
        pw    = self.piano_w
        hdr_h = 20

        # --- step header ---
        for s in range(MAX_STEPS):
            x = pw + s * CELL_W
            active = s < steps
            bg = HEADER_BG if active else '#181818'
            self.canvas.create_rectangle(x, 0, x + CELL_W, hdr_h,
                                         fill=bg, outline=GRID_LINE)
            if active:
                has_override = (self.step_velocity[s] is not None or
                                self.step_gate_ms[s]  is not None or
                                self.step_step_ms[s]  is not None)
                txt_col = '#FFD54F' if has_override else LABEL_FG
                self.canvas.create_text(x + CELL_W//2, hdr_h//2,
                                        text=str(s+1), fill=txt_col,
                                        font=('Helvetica', 8, 'bold'),
                                        tags=f'hdr_{s}')

        # --- rows ---
        for row in range(TOTAL_NOTES):
            midi   = BASE_MIDI + (TOTAL_NOTES - 1 - row)
            y      = hdr_h + row * CELL_H
            octave = midi % 12
            is_black = octave in BLACK_NOTES

            # Piano key label column
            key_bg = BLACK_KEY_BG if is_black else WHITE_KEY_BG
            self.canvas.create_rectangle(0, y, pw, y + CELL_H,
                                         fill=key_bg, outline=GRID_LINE)
            if not is_black or (midi % 12 == 0):
                self.canvas.create_text(pw - 4, y + CELL_H // 2,
                                        text=note_label(midi),
                                        anchor='e', fill=LABEL_FG,
                                        font=('Helvetica', 7))

            # Step cells
            for s in range(MAX_STEPS):
                x      = pw + s * CELL_W
                active = s < steps
                if not active:
                    fill = '#161616'
                elif self.grid_active[s][row]:
                    fill = ACTIVE_COLOR
                elif is_black:
                    fill = BLACK_KEY_BG
                else:
                    fill = GRID_BG
                self.canvas.create_rectangle(x, y, x + CELL_W, y + CELL_H,
                                             fill=fill, outline=GRID_LINE,
                                             tags=f'cell_{s}_{row}')

    def _cell_from_event(self, event):
        cx = self.canvas.canvasx(event.x)
        cy = self.canvas.canvasy(event.y)
        hdr_h = 20
        if cy < hdr_h:
            return None, None, True   # header row
        col = int((cx - self.piano_w) // CELL_W)
        row = int((cy - hdr_h) // CELL_H)
        if col < 0 or col >= MAX_STEPS or row < 0 or row >= TOTAL_NOTES:
            return None, None, False
        return col, row, False

    def _on_click(self, event):
        col, row, _ = self._cell_from_event(event)
        if col is None:
            return
        if col >= self.num_steps.get():
            return
        current = self.grid_active[col][row]
        self.grid_active[col][row] = not current
        self._drag_state = 'clear' if current else 'set'
        self._redraw_cell(col, row)

    def _on_drag(self, event):
        col, row, _ = self._cell_from_event(event)
        if col is None or self._drag_state is None:
            return
        if col >= self.num_steps.get():
            return
        target = (self._drag_state == 'set')
        if self.grid_active[col][row] != target:
            self.grid_active[col][row] = target
            self._redraw_cell(col, row)

    def _redraw_cell(self, col, row):
        tag = f'cell_{col}_{row}'
        midi   = BASE_MIDI + (TOTAL_NOTES - 1 - row)
        octave = midi % 12
        is_black = octave in BLACK_NOTES
        if self.grid_active[col][row]:
            fill = ACTIVE_COLOR
        elif is_black:
            fill = BLACK_KEY_BG
        else:
            fill = GRID_BG
        self.canvas.itemconfig(tag, fill=fill)

    def _on_right_click(self, event):
        """Per-step settings popup on right-click of a header cell."""
        cx = self.canvas.canvasx(event.x)
        cy = self.canvas.canvasy(event.y)
        if cy > 20:
            return
        col = int((cx - self.piano_w) // CELL_W)
        if col < 0 or col >= self.num_steps.get():
            return
        self._step_settings_popup(col, event.x_root, event.y_root)

    def _step_settings_popup(self, step, rx, ry):
        popup = tk.Toplevel(self)
        popup.title(f"Step {step+1} overrides")
        popup.geometry(f"+{rx}+{ry}")
        popup.configure(bg='#2A2A2A')
        popup.resizable(False, False)

        def row(lbl_text, var, from_, to_):
            fr = tk.Frame(popup, bg='#2A2A2A')
            fr.pack(fill=tk.X, padx=10, pady=3)
            tk.Label(fr, text=lbl_text, bg='#2A2A2A', fg=LABEL_FG,
                     width=12, anchor='w').pack(side=tk.LEFT)
            sb = tk.Spinbox(fr, from_=from_, to=to_, textvariable=var,
                            width=7, bg='#333', fg='white')
            sb.pack(side=tk.LEFT)

        vel_v  = tk.IntVar(value=self.step_velocity[step] or self.default_velocity.get())
        gate_v = tk.IntVar(value=self.step_gate_ms[step]  or self.default_gate_ms.get())
        step_v = tk.IntVar(value=self.step_step_ms[step]  or self.default_step_ms.get())
        use_v  = tk.BooleanVar(value=self.step_velocity[step] is not None)
        use_g  = tk.BooleanVar(value=self.step_gate_ms[step]  is not None)
        use_s  = tk.BooleanVar(value=self.step_step_ms[step]  is not None)

        def override_row(lbl_text, use_var, spin_var, from_, to_):
            fr = tk.Frame(popup, bg='#2A2A2A')
            fr.pack(fill=tk.X, padx=10, pady=3)
            tk.Checkbutton(fr, text=lbl_text, variable=use_var,
                           bg='#2A2A2A', fg=LABEL_FG, selectcolor='#555',
                           width=12, anchor='w').pack(side=tk.LEFT)
            tk.Spinbox(fr, from_=from_, to=to_, textvariable=spin_var,
                       width=7, bg='#333', fg='white').pack(side=tk.LEFT)

        override_row("Velocity",  use_v, vel_v,  1,   127)
        override_row("Gate ms",   use_g, gate_v, 10, 9999)
        override_row("Step ms",   use_s, step_v, 10, 9999)

        def apply():
            self.step_velocity[step] = vel_v.get()  if use_v.get() else None
            self.step_gate_ms[step]  = gate_v.get() if use_g.get() else None
            self.step_step_ms[step]  = step_v.get() if use_s.get() else None
            popup.destroy()
            self._draw_all()

        def clear_all():
            self.step_velocity[step] = None
            self.step_gate_ms[step]  = None
            self.step_step_ms[step]  = None
            popup.destroy()
            self._draw_all()

        btn_fr = tk.Frame(popup, bg='#2A2A2A')
        btn_fr.pack(fill=tk.X, padx=10, pady=6)
        tk.Button(btn_fr, text="Apply",     bg='#0D7377', fg='white',
                  relief='flat', command=apply).pack(side=tk.LEFT, padx=4)
        tk.Button(btn_fr, text="Clear",     bg='#555',    fg='white',
                  relief='flat', command=clear_all).pack(side=tk.LEFT)
        tk.Button(btn_fr, text="Cancel",    bg='#444',    fg='white',
                  relief='flat', command=popup.destroy).pack(side=tk.RIGHT)

    def _on_mousewheel(self, event):
        self.canvas.yview_scroll(int(-1*(event.delta/120)), "units")

    def _on_shift_mousewheel(self, event):
        self.canvas.xview_scroll(int(-1*(event.delta/120)), "units")

    def _clear(self):
        if messagebox.askyesno("Clear", "Clear all notes and overrides?"):
            self.grid_active  = [[False]*TOTAL_NOTES for _ in range(MAX_STEPS)]
            self.step_velocity = [None] * MAX_STEPS
            self.step_gate_ms  = [None] * MAX_STEPS
            self.step_step_ms  = [None] * MAX_STEPS
            self._draw_all()

    # -----------------------------------------------------------------------
    # Audio preview
    # -----------------------------------------------------------------------
    def _get_steps_data(self):
        """Return list of (note, velocity, gate_ms, step_ms) for active steps."""
        steps    = self.num_steps.get()
        def_vel  = self.default_velocity.get()
        def_gate = self.default_gate_ms.get()
        def_step = self.default_step_ms.get()
        result = []
        for s in range(steps):
            active_rows = [r for r in range(TOTAL_NOTES) if self.grid_active[s][r]]
            vel  = self.step_velocity[s] if self.step_velocity[s] is not None else def_vel
            gate = self.step_gate_ms[s]  if self.step_gate_ms[s]  is not None else def_gate
            stp  = self.step_step_ms[s]  if self.step_step_ms[s]  is not None else def_step
            if active_rows:
                midi = BASE_MIDI + (TOTAL_NOTES - 1 - active_rows[0])
                result.append((midi, vel, gate, stp))
            else:
                result.append((0, 0, 0, stp))
        return result

    def _play_preview(self):
        steps_data = self._get_steps_data()
        if not steps_data:
            self.status_var.set("Nothing to play – add some notes first.")
            return
        loop = self.loop_var.get()
        if loop:
            # repeat 4 times for preview
            steps_data = steps_data * 4
        self._player.play(steps_data)
        self._play_btn.config(bg='#1B5E20', text="▶  Playing…")
        self.status_var.set("Playing preview… Press ■ Stop to stop.")
        self._poll_player()

    def _poll_player(self):
        if self._player.is_playing():
            self.after(200, self._poll_player)
        else:
            self._play_btn.config(bg='#2E7D32', text="▶  Play")
            self.status_var.set("Playback finished.")

    def _stop_preview(self):
        self._player.stop()
        self._play_btn.config(bg='#2E7D32', text="▶  Play")
        self.status_var.set("Stopped.")

    def _on_close(self):
        self._player.stop()
        self.destroy()

    # -----------------------------------------------------------------------
    # C code generation
    # -----------------------------------------------------------------------
    def _build_c_array(self) -> str:
        steps      = self.num_steps.get()
        def_vel    = self.default_velocity.get()
        def_gate   = self.default_gate_ms.get()
        def_step   = self.default_step_ms.get()
        loop_val   = '1' if self.loop_var.get() else '0'
        arr_name   = self.array_name_var.get().strip() or "s_sequence"

        lines = []
        lines.append(f"/* Auto-generated by seq_editor.py */")
        lines.append(f"static const SequenceStep {arr_name}[] = {{")
        lines.append(f"    /* note  vel  gate_ms  step_ms */")

        for s in range(steps):
            # Find the highest active note in this column (single note per step)
            active_rows = [r for r in range(TOTAL_NOTES) if self.grid_active[s][r]]
            if active_rows:
                # Use the topmost selected note (lowest row index = highest pitch)
                row  = active_rows[0]
                midi = BASE_MIDI + (TOTAL_NOTES - 1 - row)
                vel  = self.step_velocity[s] if self.step_velocity[s] is not None else def_vel
                gate = self.step_gate_ms[s]  if self.step_gate_ms[s]  is not None else def_gate
                stp  = self.step_step_ms[s]  if self.step_step_ms[s]  is not None else def_step
                name = note_label(midi)
                hz   = midi_to_hz(midi)
                lines.append(
                    f"    {{ {midi:3d},  {vel:3d},  {gate:4d},    {stp:4d}  }},   "
                    f"/* {name:<4s} ~{hz:.1f} Hz */"
                )
            else:
                stp = self.step_step_ms[s] if self.step_step_ms[s] is not None else def_step
                lines.append(
                    f"    {{   0,    0,     0,    {stp:4d}  }},   /* rest */"
                )

        lines.append("};")
        lines.append("")
        lines.append(f"/* Loop: {loop_val}  |  Default vel:{def_vel}  gate:{def_gate}ms  step:{def_step}ms */")
        lines.append(f"/* Sequencer_Init({arr_name},")
        lines.append(f"                 sizeof({arr_name}) / sizeof({arr_name}[0]),")
        lines.append(f"                 {loop_val} /''* loop *''/ ); */")
        return '\n'.join(lines)

    def _show_output(self):
        code = self._build_c_array()

        win = tk.Toplevel(self)
        win.title("Generated C Code")
        win.configure(bg='#1A1A1A')
        win.geometry("640x420")

        txt = tk.Text(win, bg='#1E1E1E', fg='#D4D4D4',
                      font=('Courier New', 10), relief='flat',
                      insertbackground='white', wrap='none')
        txt.pack(fill=tk.BOTH, expand=True, padx=8, pady=8)
        txt.insert('1.0', code)
        txt.config(state='disabled')

        def copy_to_clipboard():
            self.clipboard_clear()
            self.clipboard_append(code)
            self.status_var.set("Copied to clipboard!")

        btn_fr = tk.Frame(win, bg='#1A1A1A')
        btn_fr.pack(fill=tk.X, padx=8, pady=(0, 8))
        tk.Button(btn_fr, text="Copy to Clipboard", bg='#0D7377', fg='white',
                  relief='flat', padx=10, command=copy_to_clipboard).pack(side=tk.LEFT)
        tk.Button(btn_fr, text="Close", bg='#444', fg='white',
                  relief='flat', padx=10, command=win.destroy).pack(side=tk.RIGHT)


# ---------------------------------------------------------------------------
if __name__ == '__main__':
    app = SeqEditor()
    # Scroll to show middle octaves on startup
    app.update_idletasks()
    app.canvas.yview_moveto(0.35)
    app.mainloop()
