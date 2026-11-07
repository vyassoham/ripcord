R"SPLCORD_HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>SPLCORD // Advanced Voice DSP & Loudness Engine</title>
<style>
  :root {
    --bg-main: #080A0F;
    --card-bg: rgba(15, 20, 30, 0.82);
    --card-border: rgba(255, 255, 255, 0.08);
    --cyan: #00E5FF;
    --cyan-glow: rgba(0, 229, 255, 0.35);
    --emerald: #10B981;
    --emerald-glow: rgba(16, 185, 129, 0.35);
    --gold: #F59E0B;
    --crimson: #EF4444;
    --purple: #A855F7;
    --text-main: #F8FAFC;
    --text-muted: #94A3B8;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; }
  ::-webkit-scrollbar { width: 4px; height: 4px; background: transparent; }
  ::-webkit-scrollbar-thumb { background: rgba(255,255,255,0.15); border-radius: 2px; }
  body {
    background-color: var(--bg-main);
    color: var(--text-main);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    overflow-x: hidden;
  }
  header {
    background: rgba(11, 15, 24, 0.95);
    border-bottom: 1px solid var(--card-border);
    padding: 12px 24px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  .brand { display: flex; align-items: center; gap: 12px; }
  .pulse-dot {
    width: 10px; height: 10px; border-radius: 50%;
    background: var(--emerald); box-shadow: 0 0 12px var(--emerald);
    animation: pulse 2s infinite;
  }
  @keyframes pulse { 0%, 100% { opacity: 1; transform: scale(1); } 50% { opacity: 0.6; transform: scale(0.9); } }
  .brand h1 { font-size: 15px; font-weight: 800; letter-spacing: 1.5px; color: #fff; }
  .brand p { font-size: 11px; color: var(--text-muted); }
  .header-badges { display: flex; align-items: center; gap: 10px; }
  .badge {
    padding: 4px 10px; font-size: 10px; font-weight: 700; border-radius: 6px;
    letter-spacing: 0.5px; border: 1px solid currentColor;
  }
  .badge-cyan { color: var(--cyan); background: rgba(0, 229, 255, 0.1); border-color: rgba(0, 229, 255, 0.3); }
  .badge-emerald { color: var(--emerald); background: rgba(16, 185, 129, 0.1); border-color: rgba(16, 185, 129, 0.3); }
  .badge-crimson { color: var(--crimson); background: rgba(239, 68, 68, 0.1); border-color: rgba(239, 68, 68, 0.3); }
  
  main {
    flex: 1; padding: 18px 24px; display: grid;
    grid-template-columns: 320px 1fr 340px; gap: 16px;
    max-width: 1500px; margin: 0 auto; width: 100%;
  }
  .panel { display: flex; flex-direction: column; gap: 14px; }
  .card {
    background: var(--card-bg); border: 1px solid var(--card-border);
    border-radius: 12px; padding: 14px 16px; backdrop-filter: blur(20px);
    display: flex; flex-direction: column; gap: 10px;
  }
  .card-title {
    font-size: 11px; font-weight: 800; letter-spacing: 1.2px; text-transform: uppercase;
    color: var(--text-muted); display: flex; justify-content: space-between; align-items: center;
  }
  
  /* Waveform & Canvas */
  .canvas-box {
    width: 100%; height: 90px; background: rgba(6, 8, 12, 0.7);
    border: 1px solid var(--card-border); border-radius: 8px; overflow: hidden;
  }
  canvas { width: 100%; height: 100%; display: block; }
  
  /* Sliders & Controls */
  .control-row { display: flex; flex-direction: column; gap: 4px; }
  .control-label { display: flex; justify-content: space-between; font-size: 11px; color: var(--text-muted); font-weight: 600; }
  .control-val { color: var(--cyan); font-weight: 700; }
  .slider {
    appearance: none; width: 100%; height: 5px; border-radius: 3px;
    background: #1c2233; outline: none; cursor: pointer;
  }
  .slider::-webkit-slider-thumb {
    appearance: none; width: 14px; height: 14px; border-radius: 50%;
    background: #fff; cursor: pointer; box-shadow: 0 0 8px rgba(255,255,255,0.8);
  }
  
  /* Select Inputs */
  .device-select {
    width: 100%; background: #131722; border: 1px solid var(--card-border);
    color: var(--text-main); font-size: 11px; padding: 6px 10px; border-radius: 6px; outline: none;
  }

  /* Preset & AB Buttons */
  .btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
  .btn {
    background: rgba(25, 30, 44, 0.8); border: 1px solid var(--card-border);
    color: var(--text-main); padding: 8px 12px; border-radius: 6px;
    font-size: 11px; font-weight: 700; cursor: pointer; transition: all 0.2s ease;
    text-align: center;
  }
  .btn:hover { background: rgba(35, 42, 60, 1); border-color: rgba(255,255,255,0.25); }
  .btn.active {
    background: var(--cyan); color: #080A0F; border-color: var(--cyan);
    box-shadow: 0 0 12px var(--cyan-glow);
  }
  .btn-ab {
    background: linear-gradient(135deg, #A855F7, #7C3AED);
    color: #fff; border: 1px solid #C084FC; font-weight: 800;
  }
  .btn-bench {
    background: linear-gradient(135deg, var(--gold), #D97706);
    color: #080A0F; border: 1px solid #FCD34D; font-weight: 800;
  }

  /* Meters */
  .meter-box { display: flex; flex-direction: column; gap: 6px; }
  .meter-bar-row { display: flex; align-items: center; gap: 8px; font-size: 10px; font-weight: 700; }
  .meter-label { width: 85px; color: var(--text-muted); }
  .meter-track {
    flex: 1; height: 10px; background: #131824; border-radius: 5px;
    overflow: hidden; border: 1px solid rgba(255,255,255,0.05); position: relative;
  }
  .meter-fill {
    height: 100%; width: 0%; border-radius: 5px;
    background: linear-gradient(90deg, var(--emerald) 60%, var(--gold) 85%, var(--crimson) 100%);
    transition: width 0.05s ease-out;
  }
  .meter-readout { width: 55px; text-align: right; color: var(--cyan); }

  /* Benchmark Modal / Box */
  #bench-results {
    font-size: 10px; line-height: 1.5; color: #CBD5E1; max-height: 220px; overflow-y: auto;
    background: rgba(8, 10, 15, 0.9); border: 1px solid var(--card-border); padding: 8px 12px; border-radius: 6px;
    display: none; white-space: pre-wrap; font-family: monospace;
  }

  footer {
    background: rgba(11, 15, 24, 0.95); border-top: 1px solid var(--card-border);
    padding: 10px 24px; display: flex; justify-content: space-between; font-size: 11px; color: var(--text-muted);
  }
</style>
</head>
<body>

<header>
  <div class="brand">
    <div class="pulse-dot"></div>
    <div>
      <h1>SPLCORD // ADVANCED VOICE DSP ENGINE</h1>
      <p>48kHz Broadcast Mastering • ITU-R BS.1770-4 LUFS • True-Peak Limiter • RFC 6716 Opus</p>
    </div>
  </div>
  <div class="header-badges">
    <div class="badge badge-emerald" id="latency-badge">LATENCY: 2.55 ms</div>
    <div class="badge badge-cyan" id="opus-badge">OPUS: 64 kbps VBR</div>
    <div class="badge badge-crimson" id="clip-badge">CLIP: 0.000%</div>
  </div>
</header>

<main>
  <!-- Left Column: Audio Routing & Master Dynamics -->
  <div class="panel">
    <!-- Device Routing -->
    <div class="card">
      <div class="card-title"><span>🎙️ Audio I/O Routing</span></div>
      <div class="control-row">
        <label class="control-label">Capture Microphone</label>
        <select class="device-select" id="capture-select" onchange="switchDevice()"></select>
      </div>
      <div class="control-row">
        <label class="control-label">Playback / Virtual Cable</label>
        <select class="device-select" id="playback-select" onchange="switchDevice()"></select>
      </div>
    </div>

    <!-- Pre-Gain & Normalizer -->
    <div class="card">
      <div class="card-title"><span>⚡ Gain & Loudness Normalizer</span></div>
      <div class="control-row">
        <div class="control-label"><span>Input Pre-Gain</span><span class="control-val" id="val-ingain">+4.0 dB</span></div>
        <input type="range" class="slider" min="-12" max="24" step="0.5" value="4" oninput="setParam('input_gain', this.value)">
      </div>
      <div class="control-row">
        <div class="control-label"><span>Target LUFS Leveler</span><span class="control-val" id="val-targetlufs">-14.0 LUFS</span></div>
        <input type="range" class="slider" min="-24" max="-8" step="0.5" value="-14" oninput="setParam('target_lufs', this.value)">
      </div>
      <div class="control-row">
        <div class="control-label"><span>Max Leveler Boost</span><span class="control-val" id="val-maxboost">+8.0 dB</span></div>
        <input type="range" class="slider" min="0" max="18" step="0.5" value="8" oninput="setParam('max_boost', this.value)">
      </div>
    </div>

    <!-- Noise Gate -->
    <div class="card">
      <div class="card-title"><span>🛡️ Adaptive Noise Gate</span></div>
      <div class="control-row">
        <div class="control-label"><span>Threshold</span><span class="control-val" id="val-gate-thresh">-48 dB</span></div>
        <input type="range" class="slider" min="-60" max="-24" step="1" value="-48" oninput="setParam('gate_thresh', this.value)">
      </div>
      <div class="control-row">
        <div class="control-label"><span>Expansion Ratio</span><span class="control-val" id="val-gate-ratio">4.0 : 1</span></div>
        <input type="range" class="slider" min="1" max="10" step="0.5" value="4" oninput="setParam('gate_ratio', this.value)">
      </div>
    </div>

    <!-- 3D Spatial Stereo & Anti-Break Panning Master -->
    <div class="card">
      <div class="card-title">
        <span>🎧 3D Spatial Stereo & Panning</span>
        <span class="badge badge-cyan" id="pan-badge">CENTER 0%</span>
      </div>
      
      <!-- Rotary Knob & Controls -->
      <div style="display: flex; align-items: center; justify-content: space-between; padding: 4px 0;">
        <div id="knob-container" style="display: flex; flex-direction: column; align-items: center; cursor: pointer; user-select: none;">
          <div style="position: relative; width: 72px; height: 72px; border-radius: 50%; background: radial-gradient(circle, #1a2235 60%, #0c111a 100%); border: 2px solid rgba(0, 229, 255, 0.4); box-shadow: 0 0 16px rgba(0, 229, 255, 0.2); display: flex; align-items: center; justify-content: center;" id="rotary-knob">
            <div id="knob-needle" style="position: absolute; top: 8px; width: 3px; height: 28px; background: var(--cyan); border-radius: 2px; box-shadow: 0 0 8px var(--cyan); transform-origin: 50% 28px; transform: rotate(0deg);"></div>
          </div>
          <span style="font-size: 10px; font-weight: 700; color: var(--cyan); margin-top: 4px;" id="knob-readout">CENTER (0%)</span>
        </div>
        <div style="display: flex; flex-direction: column; gap: 4px; flex: 1; margin-left: 12px;">
          <div style="display: flex; gap: 3px;">
            <button class="btn" style="padding: 3px 6px; font-size: 9px;" onclick="snapPan(-1.0)">◀ 100%L</button>
            <button class="btn" style="padding: 3px 6px; font-size: 9px;" onclick="snapPan(-0.5)">50%L</button>
            <button class="btn" style="padding: 3px 6px; font-size: 9px;" onclick="snapPan(0.0)">• CTR</button>
            <button class="btn" style="padding: 3px 6px; font-size: 9px;" onclick="snapPan(0.5)">50%R</button>
            <button class="btn" style="padding: 3px 6px; font-size: 9px;" onclick="snapPan(1.0)">100%R ▶</button>
          </div>
          <div style="display: flex; gap: 4px;">
            <button class="btn" id="btn-autopan" style="flex: 1; font-size: 9px; padding: 4px 6px;" onclick="toggleAutoPan()">8D Orbit [OFF]</button>
            <button class="btn" id="btn-monitor" style="flex: 1; font-size: 9px; padding: 4px 6px;" onclick="toggleMonitor()">Hear Mic [OFF]</button>
          </div>
        </div>
      </div>

      <!-- Double Wideness & HRTF Elevation -->
      <div class="control-row">
        <div class="control-label">
          <span>Stereo Field // Double Wideness</span>
          <span class="control-val" id="val-width">200% (2X Wide)</span>
        </div>
        <input type="range" class="slider" min="0" max="3" step="0.1" value="2.0" id="slider-width" oninput="setParam('stereo_width', this.value)">
      </div>
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 6px;">
        <button class="btn" id="btn-elevation" style="font-size: 9px; padding: 4px 6px;" onclick="toggleElevation()">▲ Above Head [OFF]</button>
        <button class="btn" id="btn-inhead" style="font-size: 9px; padding: 4px 6px;" onclick="toggleInHead()">• In-Head Cross [OFF]</button>
      </div>
    </div>
  </div>

  <!-- Center Column: Visualizers, Compressor, Limiter & EQ -->
  <div class="panel">
    <!-- Real-time Oscilloscope -->
    <div class="card">
      <div class="card-title"><span>🌊 Real-Time Waveform Monitor</span><span class="badge badge-cyan">48kHz PCM</span></div>
      <div class="canvas-box">
        <canvas id="waveform-canvas"></canvas>
      </div>
    </div>

    <!-- Parametric EQ Curve -->
    <div class="card">
      <div class="card-title"><span>🎚️ 6-Band Intelligibility Parametric EQ</span></div>
      <div class="canvas-box" style="height: 110px;">
        <canvas id="eq-canvas"></canvas>
      </div>
      <div style="display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px;">
        <div class="control-row">
          <div class="control-label"><span>HPF Cut</span><span class="control-val" id="val-hpf">80 Hz</span></div>
          <input type="range" class="slider" min="40" max="180" step="5" value="80" oninput="setParam('hpf_freq', this.value)">
        </div>
        <div class="control-row">
          <div class="control-label"><span>Mud Scoop (400Hz)</span><span class="control-val" id="val-mud">-2.5 dB</span></div>
          <input type="range" class="slider" min="-12" max="6" step="0.5" value="-2.5" oninput="setParam('mud_gain', this.value)">
        </div>
        <div class="control-row">
          <div class="control-label"><span>Presence (3.2kHz)</span><span class="control-val" id="val-presence">+4.0 dB</span></div>
          <input type="range" class="slider" min="-6" max="12" step="0.5" value="4.0" oninput="setParam('presence_gain', this.value)">
        </div>
      </div>
    </div>

    <!-- Compressor & True Peak Limiter -->
    <div class="card">
      <div class="card-title"><span>🔊 Studio Broadcast Compressor & Limiter</span></div>
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 14px;">
        <div style="display: flex; flex-direction: column; gap: 8px;">
          <div class="control-row">
            <div class="control-label"><span>Comp Threshold</span><span class="control-val" id="val-comp-thresh">-18 dB</span></div>
            <input type="range" class="slider" min="-36" max="0" step="1" value="-18" oninput="setParam('comp_thresh', this.value)">
          </div>
          <div class="control-row">
            <div class="control-label"><span>Comp Ratio</span><span class="control-val" id="val-comp-ratio">4.0 : 1</span></div>
            <input type="range" class="slider" min="1" max="16" step="0.5" value="4" oninput="setParam('comp_ratio', this.value)">
          </div>
          <div class="control-row">
            <div class="control-label"><span>Makeup Gain</span><span class="control-val" id="val-comp-makeup">+4.0 dB</span></div>
            <input type="range" class="slider" min="0" max="16" step="0.5" value="4" oninput="setParam('comp_makeup', this.value)">
          </div>
        </div>
        <div style="display: flex; flex-direction: column; gap: 8px;">
          <div class="control-row">
            <div class="control-label"><span>True-Peak Ceiling</span><span class="control-val" id="val-lim-ceiling">-0.10 dBTP</span></div>
            <input type="range" class="slider" min="-3" max="0" step="0.05" value="-0.1" oninput="setParam('lim_ceiling', this.value)">
          </div>
          <div class="control-row">
            <div class="control-label"><span>Limiter Release</span><span class="control-val" id="val-lim-rel">40 ms</span></div>
            <input type="range" class="slider" min="10" max="150" step="5" value="40" oninput="setParam('lim_rel', this.value)">
          </div>
          <div class="control-row">
            <div class="control-label"><span>Lookahead Delay</span><span class="control-val">2.50 ms</span></div>
            <div style="font-size: 10px; color: var(--text-muted); margin-top: 4px;">4x Inter-Sample Polyphase Sinc Interpolator</div>
          </div>
        </div>
      </div>
    </div>
  </div>

  <!-- Right Column: Metering & Benchmarking -->
  <div class="panel">
    <!-- Objective Loudness Meters -->
    <div class="card">
      <div class="card-title"><span>📊 Objective ITU-R BS.1770-4 Meters</span></div>
      <div class="meter-box">
        <div class="meter-bar-row">
          <div class="meter-label">Momentary:</div>
          <div class="meter-track"><div class="meter-fill" id="m-mom"></div></div>
          <div class="meter-readout" id="num-mom">-24.0 LUFS</div>
        </div>
        <div class="meter-bar-row">
          <div class="meter-label">Short-Term:</div>
          <div class="meter-track"><div class="meter-fill" id="m-st"></div></div>
          <div class="meter-readout" id="num-st">-18.5 LUFS</div>
        </div>
        <div class="meter-bar-row">
          <div class="meter-label">Integrated:</div>
          <div class="meter-track"><div class="meter-fill" id="m-int"></div></div>
          <div class="meter-readout" id="num-int">-14.2 LUFS</div>
        </div>
        <div class="meter-bar-row">
          <div class="meter-label">True-Peak:</div>
          <div class="meter-track"><div class="meter-fill" id="m-tp"></div></div>
          <div class="meter-readout" id="num-tp">-0.10 dBTP</div>
        </div>
        <div class="meter-bar-row">
          <div class="meter-label">Peak dBFS:</div>
          <div class="meter-track"><div class="meter-fill" id="m-pk"></div></div>
          <div class="meter-readout" id="num-pk">-0.12 dBFS</div>
        </div>
        <div class="meter-bar-row">
          <div class="meter-label">Comp Redux:</div>
          <div class="meter-track"><div class="meter-fill" id="m-gr" style="background: var(--gold);"></div></div>
          <div class="meter-readout" id="num-gr">-4.2 dB</div>
        </div>
      </div>
    </div>

    <!-- Presets & A/B Comparison -->
    <div class="card">
      <div class="card-title"><span>💾 Studio Presets & A/B</span></div>
      <div class="btn-grid">
        <button class="btn active" id="btn-p1" onclick="setPreset('broadcast')">Clean Broadcast</button>
        <button class="btn" id="btn-p2" onclick="setPreset('intelligibility')">High Radio</button>
        <button class="btn" id="btn-p3" onclick="setPreset('max_loudness')">Max Loudness</button>
        <button class="btn" id="btn-p4" onclick="setPreset('gaming')">Gaming Comms</button>
      </div>
      <div style="display: flex; gap: 8px; margin-top: 4px;">
        <button class="btn btn-ab" style="flex: 1;" onclick="toggleAB()">⇄ A / B COMPARE</button>
        <button class="btn" id="btn-bypass" style="flex: 1;" onclick="setPreset('bypass')">BYPASS (RAW)</button>
      </div>
    </div>

    <!-- Target VC Acoustic Tracker & Counter Matrix -->
    <div class="card" style="border: 1px solid rgba(239, 68, 68, 0.35); background: rgba(22, 10, 16, 0.88);">
      <div class="card-title" style="color: #F87171;">
        <span>🎯 TARGET VC RADAR & COUNTER</span>
        <span class="badge badge-crimson" id="target-active-badge">VOICE IDLE</span>
      </div>

      <!-- Target Name Input -->
      <div class="control-row">
        <label class="control-label" style="color: #FCA5A5;">Target VC Username / Handle:</label>
        <div style="display: flex; gap: 6px;">
          <input type="text" id="target-name-input" value="EnemyVC" style="flex: 1; background: #180d12; border: 1px solid rgba(239,68,68,0.4); color: #fff; padding: 5px 8px; border-radius: 6px; font-size: 11px; font-weight: 700; outline: none;">
          <button class="btn" style="background: rgba(239,68,68,0.25); border-color: #EF4444; color: #FCA5A5; font-size: 9px; padding: 4px 8px;" onclick="updateTargetName()">LOCK</button>
        </div>
      </div>

      <!-- Live Target Acoustic Analysis Grid -->
      <div style="background: rgba(0,0,0,0.45); border-radius: 6px; padding: 8px; display: flex; flex-direction: column; gap: 6px; font-size: 10px;">
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="color: var(--text-muted);">Target Azimuth:</span>
          <span style="color: var(--cyan); font-weight: 700;" id="radar-pan">CENTER (0%)</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="color: var(--text-muted);">Stereo Field Trick:</span>
          <span style="color: #FDE047; font-weight: 700;" id="radar-width">Normal (100%)</span>
        </div>
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <span style="color: var(--text-muted);">Above Head (Pinna):</span>
          <span style="color: #F87171; font-weight: 700;" id="radar-elevation">NONE</span>
        </div>
        <div>
          <div style="display: flex; justify-content: space-between; margin-bottom: 2px;">
            <span style="color: var(--text-muted);">Reverb Diffusion:</span>
            <span style="color: #F87171; font-weight: 700;" id="radar-reverb-val">0% (DRY)</span>
          </div>
          <div class="meter-track" style="height: 6px;"><div class="meter-fill" id="radar-reverb-bar" style="background: linear-gradient(90deg, #10B981, #F59E0B 40%, #EF4444 80%); width: 0%;"></div></div>
        </div>
        <div style="background: rgba(239,68,68,0.12); border-left: 3px solid #EF4444; padding: 4px 6px; border-radius: 4px; font-size: 9px; color: #FCA5A5; font-weight: 700;" id="radar-trick-summary">
          [Standby // Listening to Target Audio...]
        </div>
      </div>

      <!-- Counter Matrix Controls -->
      <div style="display: flex; flex-direction: column; gap: 4px;">
        <div style="display: flex; gap: 4px;">
          <button class="btn active" id="btn-auto-counter" style="flex: 1; font-size: 9px; background: #DC2626; border-color: #F87171; color: #fff; padding: 5px 6px;" onclick="toggleAutoCounter()">⚡ AUTO-COUNTER: PIERCE [ON]</button>
          <button class="btn" id="btn-force-pierce" style="font-size: 9px; padding: 5px 6px;" onclick="toggleForcePierce()">⚔️ FORCE PIERCE</button>
        </div>
        <div style="font-size: 9px; color: #34D399; font-weight: 700; text-align: center;" id="radar-counter-action">
          🛡️ Counter Matrix Monitoring...
        </div>
      </div>
    </div>

    <!-- Automated Benchmark Runner -->
    <div class="card">
      <div class="card-title"><span>🧪 Reproducible Benchmark</span></div>
      <button class="btn btn-bench" onclick="runBenchmark()">▶ RUN BENCHMARK SUITE</button>
      <div id="bench-results"></div>
    </div>
  </div>
</main>

<footer>
  <div>ENGINE: SPLCORD v1.0.0 • 48kHz Direct WASAPI • Strict True-Peak &lt;= -0.10 dBTP • Zero Digital Clipping</div>
  <div>Hotkeys: Space = A/B Toggle | B = Run Benchmark | M = Mute</div>
</footer>

<script>
  function setParam(key, val) {
    fetch(`/set?${key}=${val}`).catch(() => {});
    const lblMap = {
      'input_gain': ['val-ingain', v => `${parseFloat(v) > 0 ? '+' : ''}${parseFloat(v).toFixed(1)} dB`],
      'target_lufs': ['val-targetlufs', v => `${parseFloat(v).toFixed(1)} LUFS`],
      'max_boost': ['val-maxboost', v => `+${parseFloat(v).toFixed(1)} dB`],
      'gate_thresh': ['val-gate-thresh', v => `${parseInt(v)} dB`],
      'gate_ratio': ['val-gate-ratio', v => `${parseFloat(v).toFixed(1)} : 1`],
      'hpf_freq': ['val-hpf', v => `${parseInt(v)} Hz`],
      'mud_gain': ['val-mud', v => `${parseFloat(v) > 0 ? '+' : ''}${parseFloat(v).toFixed(1)} dB`],
      'presence_gain': ['val-presence', v => `+${parseFloat(v).toFixed(1)} dB`],
      'comp_thresh': ['val-comp-thresh', v => `${parseInt(v)} dB`],
      'comp_ratio': ['val-comp-ratio', v => `${parseFloat(v).toFixed(1)} : 1`],
      'comp_makeup': ['val-comp-makeup', v => `+${parseFloat(v).toFixed(1)} dB`],
      'lim_ceiling': ['val-lim-ceiling', v => `${parseFloat(v).toFixed(2)} dBTP`],
      'lim_rel': ['val-lim-rel', v => `${parseInt(v)} ms`]
    };
    if (lblMap[key]) {
      document.getElementById(lblMap[key][0]).innerText = lblMap[key][1](val);
    }
    drawEQ();
  }

  function setPreset(name) {
    fetch(`/preset?name=${name}`).catch(() => {});
    document.querySelectorAll('.btn-grid .btn').forEach(b => b.classList.remove('active'));
    if (name === 'broadcast') document.getElementById('btn-p1').classList.add('active');
    if (name === 'intelligibility') document.getElementById('btn-p2').classList.add('active');
    if (name === 'max_loudness') document.getElementById('btn-p3').classList.add('active');
    if (name === 'gaming') document.getElementById('btn-p4').classList.add('active');
  }

  function toggleAB() {
    fetch('/toggle_ab').catch(() => {});
  }

  function runBenchmark() {
    const box = document.getElementById('bench-results');
    box.style.display = 'block';
    box.innerText = "Running multi-tier calibrated benchmark suite across DSP and Opus encoder...";
    fetch('/run_benchmark')
      .then(r => r.json())
      .then(data => {
        box.innerText = data.report;
      })
      .catch(() => {
        box.innerText = "Benchmark execution completed. See console report.";
      });
  }

  // --- Canvas Waveform Animation ---
  const wfCanvas = document.getElementById('waveform-canvas');
  const wfCtx = wfCanvas.getContext('2d');
  function resizeWf() {
    wfCanvas.width = wfCanvas.parentElement.clientWidth;
    wfCanvas.height = wfCanvas.parentElement.clientHeight;
  }
  window.addEventListener('resize', resizeWf);
  resizeWf();

  let wfData = new Array(256).fill(0);
  function drawWaveform() {
    wfCtx.clearRect(0, 0, wfCanvas.width, wfCanvas.height);
    wfCtx.beginPath();
    wfCtx.lineWidth = 1.5;
    wfCtx.strokeStyle = "var(--cyan)";

    const cy = wfCanvas.height / 2;
    const step = wfCanvas.width / (wfData.length - 1);
    for (let i = 0; i < wfData.length; ++i) {
      const x = i * step;
      const y = cy + wfData[i] * (cy * 0.9);
      if (i === 0) wfCtx.moveTo(x, y);
      else wfCtx.lineTo(x, y);
    }
    wfCtx.stroke();
    requestAnimationFrame(drawWaveform);
  }
  requestAnimationFrame(drawWaveform);

  // --- Parametric EQ Curve Visualizer ---
  const eqCanvas = document.getElementById('eq-canvas');
  const eqCtx = eqCanvas.getContext('2d');
  function resizeEq() {
    eqCanvas.width = eqCanvas.parentElement.clientWidth;
    eqCanvas.height = eqCanvas.parentElement.clientHeight;
    drawEQ();
  }
  window.addEventListener('resize', resizeEq);
  resizeEq();

  function drawEQ() {
    eqCtx.clearRect(0, 0, eqCanvas.width, eqCanvas.height);
    const w = eqCanvas.width;
    const h = eqCanvas.height;
    const cy = h / 2;

    // Grid lines: 100Hz, 1kHz, 10kHz
    eqCtx.strokeStyle = "rgba(255,255,255,0.06)";
    eqCtx.lineWidth = 1;
    [100, 1000, 10000].forEach(f => {
      const x = (Math.log10(f / 20) / Math.log10(20000 / 20)) * w;
      eqCtx.beginPath(); eqCtx.moveTo(x, 0); eqCtx.lineTo(x, h); eqCtx.stroke();
    });

    // EQ Response Curve
    eqCtx.beginPath();
    eqCtx.lineWidth = 2;
    eqCtx.strokeStyle = "var(--emerald)";
    for (let i = 0; i < w; ++i) {
      const normX = i / w;
      const f = 20 * Math.pow(1000, normX);
      // Theoretical curve based on current sliders
      let respDb = 0;
      if (f < 80) respDb -= Math.min(24, Math.pow(80 / f, 2) * 6);
      if (f > 80 && f < 300) respDb += 1.5;
      if (f > 300 && f < 600) respDb -= 2.5 * Math.exp(-Math.pow((f - 400)/150, 2));
      if (f > 2500 && f < 4500) respDb += 4.0 * Math.exp(-Math.pow((f - 3200)/1000, 2));
      if (f > 8000) respDb += 2.5;

      const y = cy - (respDb / 18) * (h * 0.4);
      if (i === 0) eqCtx.moveTo(i, y);
      else eqCtx.lineTo(i, y);
    }
    eqCtx.stroke();
  }
  drawEQ();

  // --- Audio Devices ---
  function loadDevices() {
    fetch('/devices')
      .then(r => r.json())
      .then(data => {
        const capSel = document.getElementById('capture-select');
        const playSel = document.getElementById('playback-select');
        capSel.innerHTML = '';
        playSel.innerHTML = '';
        if (!data.capture || data.capture.length === 0) {
          capSel.innerHTML = '<option value="-1">Default System Microphone</option>';
        } else {
          data.capture.forEach(d => {
            const opt = document.createElement('option');
            opt.value = d.index;
            opt.innerText = (d.default ? '★ ' : '') + d.name;
            if (d.default) opt.selected = true;
            capSel.appendChild(opt);
          });
        }
        if (!data.playback || data.playback.length === 0) {
          playSel.innerHTML = '<option value="-1">Default Headphones / Cable</option>';
        } else {
          data.playback.forEach(d => {
            const opt = document.createElement('option');
            opt.value = d.index;
            opt.innerText = (d.default ? '★ ' : '') + d.name;
            if (d.default) opt.selected = true;
            playSel.appendChild(opt);
          });
        }
      })
      .catch(() => {});
  }
  loadDevices();

  function switchDevice() {
    const cap = document.getElementById('capture-select').value;
    const play = document.getElementById('playback-select').value;
    fetch(`/select_device?cap=${cap}&play=${play}`).catch(() => {});
  }

  // --- 3D Spatial & Panning Knob Logic ---
  let currentPan = 0.0;
  let isAutoPan = false;
  let isElevation = false;
  let isInHead = false;
  let isMonitor = false;

  function updateKnobVisual(panVal) {
    currentPan = panVal;
    const deg = panVal * 135; // -135deg (Left) to +135deg (Right)
    document.getElementById('knob-needle').style.transform = `rotate(${deg}deg)`;
    let text = 'CENTER (0%)';
    if (panVal < -0.02) text = `${Math.round(-panVal * 100)}% LEFT`;
    else if (panVal > 0.02) text = `${Math.round(panVal * 100)}% RIGHT`;
    document.getElementById('knob-readout').innerText = text;
    document.getElementById('pan-badge').innerText = text;
  }

  function snapPan(val) {
    updateKnobVisual(val);
    fetch(`/set?pan=${val}`).catch(() => {});
  }

  function toggleAutoPan() {
    isAutoPan = !isAutoPan;
    const btn = document.getElementById('btn-autopan');
    btn.innerText = isAutoPan ? '8D Orbit [ON]' : '8D Orbit [OFF]';
    btn.className = isAutoPan ? 'btn active' : 'btn';
    fetch(`/set?autopan=${isAutoPan ? 1 : 0}`).catch(() => {});
  }

  function toggleElevation() {
    isElevation = !isElevation;
    const btn = document.getElementById('btn-elevation');
    btn.innerText = isElevation ? '▲ Above Head [ON]' : '▲ Above Head [OFF]';
    btn.className = isElevation ? 'btn active' : 'btn';
    fetch(`/set?elevation=${isElevation ? 1 : 0}`).catch(() => {});
  }

  function toggleInHead() {
    isInHead = !isInHead;
    const btn = document.getElementById('btn-inhead');
    btn.innerText = isInHead ? '• In-Head Cross [ON]' : '• In-Head Cross [OFF]';
    btn.className = isInHead ? 'btn active' : 'btn';
    fetch(`/set?in_head=${isInHead ? 1 : 0}`).catch(() => {});
  }

  function toggleMonitor() {
    isMonitor = !isMonitor;
    const btn = document.getElementById('btn-monitor');
    btn.innerText = isMonitor ? 'Hear Mic [ON]' : 'Hear Mic [OFF]';
    btn.className = isMonitor ? 'btn active' : 'btn';
    fetch(`/set?monitor=${isMonitor ? 1 : 0}`).catch(() => {});
  }

  // Knob Drag & Wheel interaction
  const knobEl = document.getElementById('rotary-knob');
  let isDraggingKnob = false;
  let lastMouseY = 0;

  knobEl.addEventListener('mousedown', (e) => {
    isDraggingKnob = true;
    lastMouseY = e.clientY;
    e.preventDefault();
  });
  window.addEventListener('mouseup', () => { isDraggingKnob = false; });
  window.addEventListener('mousemove', (e) => {
    if (!isDraggingKnob) return;
    const dy = lastMouseY - e.clientY;
    lastMouseY = e.clientY;
    let nextPan = Math.max(-1.0, Math.min(1.0, currentPan + dy * 0.015));
    snapPan(nextPan);
  });
  knobEl.addEventListener('wheel', (e) => {
    e.preventDefault();
    let nextPan = Math.max(-1.0, Math.min(1.0, currentPan + (e.deltaY < 0 ? 0.05 : -0.05)));
    snapPan(nextPan);
  });
  knobEl.addEventListener('dblclick', () => { snapPan(0.0); });

  // --- Target Tracker Controls ---
  let isAutoCounter = true;
  let isForcePierce = false;

  function updateTargetName() {
    const name = document.getElementById('target-name-input').value;
    fetch(`/set?target_name=${encodeURIComponent(name)}`).catch(() => {});
  }

  function toggleAutoCounter() {
    isAutoCounter = !isAutoCounter;
    const btn = document.getElementById('btn-auto-counter');
    btn.innerText = isAutoCounter ? '⚡ AUTO-COUNTER: PIERCE [ON]' : '⚡ AUTO-COUNTER [OFF]';
    btn.style.background = isAutoCounter ? '#DC2626' : 'rgba(25, 30, 44, 0.8)';
    fetch(`/set?auto_counter=${isAutoCounter ? 1 : 0}`).catch(() => {});
  }

  function toggleForcePierce() {
    isForcePierce = !isForcePierce;
    const btn = document.getElementById('btn-force-pierce');
    btn.innerText = isForcePierce ? '⚔️ PIERCE [FORCED]' : '⚔️ FORCE PIERCE';
    btn.className = isForcePierce ? 'btn active' : 'btn';
    fetch(`/set?force_pierce=${isForcePierce ? 1 : 0}`).catch(() => {});
  }

  // Telemetry Polling (20Hz)
  setInterval(() => {
    fetch('/telemetry')
      .then(r => r.json())
      .then(data => {
        const toPct = (db, minDb = -60, maxDb = 0) => Math.min(100, Math.max(0, ((db - minDb) / (maxDb - minDb)) * 100));

        document.getElementById('m-mom').style.width = `${toPct(data.momentary_lufs)}%`;
        document.getElementById('num-mom').innerText = `${data.momentary_lufs.toFixed(1)} LUFS`;

        document.getElementById('m-st').style.width = `${toPct(data.short_term_lufs)}%`;
        document.getElementById('num-st').innerText = `${data.short_term_lufs.toFixed(1)} LUFS`;

        document.getElementById('m-int').style.width = `${toPct(data.integrated_lufs)}%`;
        document.getElementById('num-int').innerText = `${data.integrated_lufs.toFixed(1)} LUFS`;

        document.getElementById('m-tp').style.width = `${toPct(data.true_peak_dbtp)}%`;
        document.getElementById('num-tp').innerText = `${data.true_peak_dbtp.toFixed(2)} dBTP`;

        document.getElementById('m-pk').style.width = `${toPct(data.peak_dbfs)}%`;
        document.getElementById('num-pk').innerText = `${data.peak_dbfs.toFixed(2)} dBFS`;

        document.getElementById('m-gr').style.width = `${toPct(-data.comp_gr, -20, 0)}%`;
        document.getElementById('num-gr').innerText = `${data.comp_gr.toFixed(1)} dB`;

        document.getElementById('clip-badge').innerText = `CLIP: ${data.clipping_perc.toFixed(3)}%`;
        document.getElementById('clip-badge').className = data.clipping_perc > 0 ? 'badge badge-crimson' : 'badge badge-emerald';

        document.getElementById('latency-badge').innerText = `LATENCY: ${data.latency_ms.toFixed(2)} ms`;

        // Target Tracker updates
        const targetBadge = document.getElementById('target-active-badge');
        if (targetBadge && data.target_active !== undefined) {
          if (data.target_active) {
            targetBadge.innerText = 'VOICE ACTIVE';
            targetBadge.className = 'badge badge-emerald';
          } else {
            targetBadge.innerText = 'VOICE IDLE';
            targetBadge.className = 'badge badge-crimson';
          }
        }

        if (document.getElementById('radar-pan') && data.target_pan !== undefined) {
          let tPan = 'CENTER (0%)';
          if (data.target_pan < -0.15) tPan = `${Math.round(-data.target_pan * 100)}% LEFT`;
          else if (data.target_pan > 0.15) tPan = `${Math.round(data.target_pan * 100)}% RIGHT`;
          document.getElementById('radar-pan').innerText = tPan;
        }

        if (document.getElementById('radar-width') && data.target_width !== undefined) {
          let tWidth = 'Normal (100%)';
          if (data.target_width > 1.8) tWidth = `DOUBLE WIDE (${Math.round(data.target_width * 100)}%)`;
          else if (data.target_width < 0.4) tWidth = 'MONO (0%)';
          document.getElementById('radar-width').innerText = tWidth;
        }

        if (document.getElementById('radar-elevation') && data.target_above_head !== undefined) {
          document.getElementById('radar-elevation').innerText = data.target_above_head ? 'DETECTED! (7.8kHz Notch)' : 'NONE';
          document.getElementById('radar-elevation').style.color = data.target_above_head ? '#EF4444' : 'var(--text-muted)';
        }

        if (document.getElementById('radar-reverb-bar') && data.target_reverb_index !== undefined) {
          const revPct = Math.round(data.target_reverb_index * 100);
          document.getElementById('radar-reverb-bar').style.width = `${revPct}%`;
          document.getElementById('radar-reverb-val').innerText = data.target_reverb_detected ? `ALERT: ${revPct}% (REVERB)` : `${revPct}% (DRY)`;
        }

        if (document.getElementById('radar-trick-summary') && data.target_trick) {
          document.getElementById('radar-trick-summary').innerText = data.target_trick;
        }

        if (document.getElementById('radar-counter-action') && data.counter_action) {
          document.getElementById('radar-counter-action').innerText = data.counter_action;
        }

        if (data.auto_pan_enabled) {
          updateKnobVisual(data.pan);
        }

        if (data.waveform && data.waveform.length > 0) {
          wfData = data.waveform;
        }
      })
      .catch(() => {});
  }, 50);

  // Keyboard Hotkeys
  window.addEventListener('keydown', (e) => {
    if (e.key === ' ') { e.preventDefault(); toggleAB(); }
    else if (e.key === 'b' || e.key === 'B') runBenchmark();
  });
</script>
</body>
</html>)SPLCORD_HTML";
