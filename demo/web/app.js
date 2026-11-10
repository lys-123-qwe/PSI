 const $ = (id) => document.getElementById(id);
const fmt = (n) => Number(n).toLocaleString("zh-CN");
const formatLocalDuration = (seconds) => {
  const value = Number(seconds || 0);
  if (value < 0.001) return `${(value * 1e6).toFixed(1)} µs`;
  if (value < 1) return `${(value * 1000).toFixed(2)} ms`;
  return `${value.toFixed(3)} s`;
};
const escapeHtml = (value) => String(value).replace(/[&<>"']/g, (ch) => ({
  "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
}[ch]));
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// Paper 1 (IoV ride-sharing), Table IV: N=3, LAN 1000Mbps, t=0.6n, seconds.
const PAPER_IV = {
  10: { n: 1024, t: 614, s: 2.15 },
  11: { n: 2048, t: 1229, s: 7.10 },
  12: { n: 4096, t: 2458, s: 27.21 },
  13: { n: 8192, t: 4915, s: 105.95 },
  14: { n: 16384, t: 9830, s: 415.04 },
};

const state = {
  config: null,
  scenario: null,
  nLog: 20,
  threshold: 8000,
  common: 12345,
  parties: 2,
  mode: "mock",
  backend: { exists: false, patched: false, binary: "" },
  running: false,
  lastResult: null,
  runToken: 0,
  timer: null,
  progressTimer: null,
  benchData: null,
  runFailed: false,
  runMetaBase: "",
  version: null,
  selfcheck: null,
  lastRunError: null,
  dataMode: "generated",
  customThreshold: 4,
  customSets: { a: [], b: [], common: [] },
  datasetSeed: 7,
};

const PROTOCOL_STEPS = [
  ["encode", "本地编码 / 哈希", "双方各自把集合元素编码成 128-bit ID，原始元素不出域。"],
  ["oprf", "OPRF / 密钥交换", "用不经意伪随机函数对齐元素，对方无法反推你的集合。"],
  ["share", "秘密分享 / 集合大小测试", "把交集规模拆成份额，只测试“够不够”，不暴露具体元素。"],
  ["threshold", "门限比较", "把测试结果与门限 k 比较：达到才继续，否则输出 ⊥。"],
  ["output", "输出交集 / 匹配结果", "达到门限才输出交集/业务匹配；未达到则双方只知道“不够”。"],
];

const COACH_LINES = {
  encode: "第一步：两边各自在本地把用户 ID / 路线点 / 位点编码，原始数据不出域。",
  oprf: "第二步：交换 OPRF 结果，对齐元素；对方看到的是伪随机值，不是你的名单。",
  share: "第三步：把交集规模做成秘密分享，只测试“够不够”，不暴露具体是哪些元素。",
  threshold: "第四步：和门限 k 比较。这里就是老师要的“交集数量门限”。",
  output: "第五步：达到门限才输出匹配；未达到就输出 ⊥，双方只知道“不够”。",
  waiting: "等待真实后端返回日志与阶段耗时…",
};

const DEFAULT_PRIVACY_BANNER = "只交换协议消息：哈希值、OPRF 输出、秘密分享份额；不交换原始 ID。";
const CUSTOM_PRIVACY_BANNER = "手动输入只在本演示服务内计算，用于讲清 A/B 交集；真实部署应让双方各自运行客户端，原始集合不出域。";
const MAX_MANUAL_ITEMS = 200;

function stableNumber(text) {
  let hash = 2166136261;
  for (const ch of String(text)) {
    hash ^= ch.codePointAt(0);
    hash = Math.imul(hash, 16777619);
  }
  return hash >>> 0;
}

function sampleElement(sequence, salt) {
  const scenario = state.scenario ? state.scenario.id : "ad";
  const seed = state.datasetSeed + stableNumber(`${scenario}:${sequence}:${salt}`);
  const hex = seed.toString(16).padStart(8, "0").slice(0, 6);
  if (scenario === "ride") {
    return `geo_${30 + (seed % 20)}.${100 + (seed % 900)}_${120 + (seed % 20)}.${100 + ((seed >> 7) % 900)}`;
  }
  if (scenario === "gene") {
    return `locus_rs${100000 + (seed % 900000)}`;
  }
  return `uid_${hex}`;
}

function parseSetText(text) {
  const items = [];
  const seen = new Set();
  for (const raw of String(text || "").split(/[\s,，;；]+/)) {
    const value = raw.trim();
    if (!value || seen.has(value)) continue;
    if (value.length > 64) throw new Error("单个元素最多 64 个字符");
    seen.add(value);
    items.push(value);
    if (items.length > MAX_MANUAL_ITEMS) throw new Error(`每方最多 ${MAX_MANUAL_ITEMS} 条；百万级数据请用自动生成模式`);
  }
  return items;
}

function buildSampleSets() {
  const a = [];
  const b = [];
  for (let index = 1; index <= 4; index += 1) {
    const shared = sampleElement(index, "shared");
    a.push(shared);
    b.push(shared);
  }
  for (let index = 1; index <= 5; index += 1) a.push(sampleElement(index + 10, "a"));
  for (let index = 1; index <= 5; index += 1) b.push(sampleElement(index + 20, "b"));
  return { a, b };
}

function renderGeneratedDataset() {
  if (!state.scenario) return;
  const n = 2 ** state.nLog;
  const shared = Math.min(n, Math.max(0, state.common));
  const aLines = [];
  const bLines = [];
  for (let index = 1; index <= Math.min(4, shared); index += 1) {
    const value = sampleElement(index, "shared");
    aLines.push(value);
    bLines.push(value);
  }
  for (let index = 1; index <= 4; index += 1) aLines.push(sampleElement(index + 10, "a"));
  for (let index = 1; index <= 4; index += 1) bLines.push(sampleElement(index + 20, "b"));
  const more = n > 8 ? "\n… 仅展示前 8 条样例" : "";
  $("generatedALabel").textContent = state.scenario.party_a;
  $("generatedBLabel").textContent = state.scenario.party_b;
  $("generatedACount").textContent = `${fmt(n)} 条`;
  $("generatedBCount").textContent = `${fmt(n)} 条`;
  $("generatedAPreview").textContent = aLines.join("\n") + more;
  $("generatedBPreview").textContent = bLines.join("\n") + more;
  $("generatedSummary").textContent = `A 方 ${fmt(n)} 条 · B 方 ${fmt(n)} 条 · 共同元素 ${fmt(shared)} 条（合成数据）`;
}

function updateCustomCounts() {
  const count = (id) => {
    try { return parseSetText($(id).value).length; } catch (_) { return "—"; }
  };
  $("customACount").textContent = `${count("setAText")} 条`;
  $("customBCount").textContent = `${count("setBText")} 条`;
}

function renderCustomResult() {
  const { a, b, common } = state.customSets;
  $("customACount").textContent = `${fmt(a.length)} 条`;
  $("customBCount").textContent = `${fmt(b.length)} 条`;
  const preview = common.slice(0, 20).map((item) => `<code>${escapeHtml(item)}</code>`).join("");
  const more = common.length > 20 ? `<p class="hint">另有 ${fmt(common.length - 20)} 条交集，导出 JSON 可查看完整结果。</p>` : "";
  $("customResult").innerHTML = `
    <div class="custom-result-head">
      <strong>本地交集预览</strong>
      <span>A ${fmt(a.length)} 条 · B ${fmt(b.length)} 条 · 交集 ${fmt(common.length)} 条</span>
    </div>
    ${common.length ? `<div class="intersection-chips">${preview}</div>` : '<p class="intersection-empty">没有共同元素。</p>'}
    ${more}`;
  $("customResult").classList.remove("hidden");
}

function analyzeCustomSets(silent = false) {
  try {
    const a = parseSetText($("setAText").value);
    const b = parseSetText($("setBText").value);
    if (!a.length || !b.length) throw new Error("A、B 两方都至少需要一个元素");
    const bSet = new Set(b);
    const common = a.filter((item) => bSet.has(item));
    state.customSets = { a, b, common };
    const maxSize = Math.max(a.length, b.length, 1);
    state.customThreshold = Math.max(0, Math.min(maxSize, Math.round(Number(state.customThreshold || 4))));
    renderCustomResult();
    syncControls();
    if (!silent) toast(`已计算：A ${a.length} 条 / B ${b.length} 条 / 交集 ${common.length} 条`);
    return true;
  } catch (error) {
    $("customResult").classList.add("hidden");
    if (!silent) toast(error.message);
    return false;
  }
}

function updateDataModeUI() {
  const custom = state.dataMode === "custom";
  $("generatedData").classList.toggle("hidden", custom);
  $("customData").classList.toggle("hidden", !custom);
  document.querySelectorAll("#dataModeToggle button").forEach((button) => {
    button.classList.toggle("active", button.dataset.dataMode === state.dataMode);
    if (button.dataset.dataMode === "custom") {
      button.disabled = state.mode === "live";
      button.title = state.mode === "live" ? "真实后端模式下请用自动生成；真实文件请走右上角「真实数据 Demo」" : "";
    }
  });
}

function setDataMode(mode, silent = false) {
  if (mode === "custom" && state.mode === "live") {
    if (!silent) toast("手动输入只做本地演示；真实文件请走右上角「真实数据 Demo」");
    return false;
  }
  state.dataMode = mode;
  updateDataModeUI();
  if (mode === "custom") {
    if (!$("setAText").value.trim() && !$("setBText").value.trim()) {
      const sample = buildSampleSets();
      $("setAText").value = sample.a.join("\n");
      $("setBText").value = sample.b.join("\n");
    }
    analyzeCustomSets(true);
  } else {
    renderGeneratedDataset();
  }
  syncControls();
  return true;
}

function toast(message) {
  const el = $("toast");
  el.textContent = message;
  el.classList.add("show");
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => el.classList.remove("show"), 3200);
}

async function copyText(text) {
  try {
    await navigator.clipboard.writeText(text);
    return true;
  } catch (_) {
    const area = document.createElement("textarea");
    area.value = text;
    area.style.position = "fixed";
    area.style.opacity = "0";
    document.body.appendChild(area);
    area.select();
    const ok = document.execCommand("copy");
    area.remove();
    return ok;
  }
}

function switchScreen(name) {
  document.querySelectorAll(".screen").forEach((section) => {
    section.classList.toggle("active", section.id === `screen-${name}`);
  });
  document.querySelectorAll("#stepNav button").forEach((button) => {
    button.classList.toggle("active", button.dataset.target === name);
    if (button.dataset.target === "running") button.disabled = !state.running && !state.runFailed;
    if (button.dataset.target === "result") button.disabled = !state.lastResult;
  });
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function syncPartyCards() {
  const three = state.parties === 3;
  $("partyRow").classList.toggle("three-party", three);
  $("partyCDivider").classList.toggle("hidden", !three);
  $("partyCCard").classList.toggle("hidden", !three);
}

function syncPartyMode() {
  const select = $("parties");
  if (state.mode === "live") {
    state.parties = 3;
    select.value = "3";
    select.disabled = true;
  } else {
    state.parties = 2;
    select.value = "2";
    select.disabled = false;
  }
  syncPartyCards();
}

function setMode(mode) {
  const wasLive = state.mode === "live";
  state.mode = mode;
  if (mode === "live" && state.dataMode === "custom") {
    state.dataMode = "generated";
    toast("已切回自动生成模式；真实文件请走右上角「真实数据 Demo」");
  }
  document.querySelectorAll("#modeToggle button").forEach((button) => {
    button.classList.toggle("active", button.dataset.mode === mode);
  });
  $("modePill").textContent = mode === "live" ? "真实后端" : "演示模式";
  $("footerBackend").textContent = `backend: ${mode === "live" ? "live" : "mock"}`;
  if (mode === "live" && state.nLog > 17) {
    state.nLog = 17;
    toast("真实后端当前最大支持 2^17，已切到 131,072");
  }
  if (mode === "live" && !wasLive) {
    let adjusted = false;
    if (state.nLog > 11) {
      state.nLog = 11;
      adjusted = true;
    }
    if (state.threshold > 256) {
      state.threshold = 100;
      adjusted = true;
    }
    if (adjusted) toast("现场真实执行已切到 2^11 / k=100（约 8 秒）；2^20 为论文口径");
  }
  updateDataModeUI();
  syncPartyMode();
  syncControls();
}

function updateBackendNote() {
  const note = $("backendNote");
  if (state.dataMode === "custom") {
    note.classList.remove("warn");
    note.textContent = "手动输入模式：由 Python 演示后端计算 A/B 交集和门限判断，不是密码学 PSI。真实三方文件请走右上角「真实数据 Demo」。";
    return;
  }
  if (state.mode === "mock") {
    note.classList.remove("warn");
    note.textContent = "演示模式：时间按 CCS'23 论文锚定估算，适合讲流程和门限翻转；不依赖 Linux 后端。";
    return;
  }
  if (!state.backend.exists) {
    note.classList.add("warn");
    note.textContent = "未检测到真实后端。请在 Linux 上设置 MYPSI_BIN 指向 Qin-3 的 frontend 二进制（三方：角色 0/1/2），再重启本服务。当前点击“开始”会切回演示模式。";
    return;
  }
  note.classList.remove("warn");
  const patchNote = state.backend.patched
    ? "已启用独立实际交集参数，可演示“未达到门限”。"
    : "当前二进制把门限同时当作实际交集，只能演示“达到门限”；拒绝分支需应用 patches/ 下补丁。";
  const stat = state.backend.binary_mtime
    ? `（${state.backend.binary_mtime}，${(Number(state.backend.binary_size || 0) / 1048576).toFixed(2)} MB）`
    : "";
  note.textContent = `真实后端：${state.backend.binary}${stat}。三方 Qin-3 协议（角色 0/1/2，端口 1212/1213）。${patchNote} 需要接真实文件时，用右上角「真实数据 Demo」。`;
}

function scenarioById(id) {
  return state.config.scenarios.find((item) => item.id === id);
}

function scenarioRoleText(scenario) {
  if (!scenario) return "";
  const roles = [`A ${scenario.party_a}`, `B ${scenario.party_b}`];
  if (scenario.party_c) roles.push(`C ${scenario.party_c}`);
  return roles.join(" / ");
}

function scenarioExampleText(scenario) {
  if (!scenario || !scenario.example) return "";
  const example = scenario.example;
  return `业务示例（合成/示意）：${scenario.data_a} ${fmt(example.size_a)} / ${scenario.data_b} ${fmt(example.size_b)} / ${scenario.data_c} ${fmt(example.size_c)} → 共同 ${fmt(example.intersection)}；${scenario.threshold_label} = ${fmt(example.threshold)} 时通过。`;
}

function renderScenarios() {
  const grid = $("scenarioGrid");
  grid.innerHTML = "";
  state.config.scenarios.forEach((scenario) => {
    const card = document.createElement("button");
    card.className = "scenario-card";
    card.innerHTML = `
      <div class="scenario-icon">${scenario.icon}</div>
      <h3>${scenario.name}</h3>
      <p>${scenario.tagline}</p>
      <div class="scenario-facts">
        <div><span>三方数据</span><b>${scenario.data_a} / ${scenario.data_b} / ${scenario.data_c}</b></div>
        <div><span>元素格式</span><b>${scenario.element_format}</b></div>
        <div><span>门限含义</span><b>${scenario.threshold_label}</b></div>
      </div>
      <div class="scenario-example">${scenarioExampleText(scenario)}</div>
      <div class="scenario-meta">
        <span class="chip">A：${scenario.party_a}</span>
        <span class="chip">B：${scenario.party_b}</span>
        <span class="chip">C：${scenario.party_c}</span>
      </div>
      <div class="scenario-cta">开始配置 →</div>`;
    card.addEventListener("click", () => chooseScenario(scenario.id));
    grid.appendChild(card);
  });

  const disabledGrid = $("disabledGrid");
  disabledGrid.innerHTML = "";
  state.config.disabled.forEach((scenario) => {
    const card = document.createElement("button");
    card.className = "scenario-card disabled";
    card.innerHTML = `
      <div class="scenario-icon">${scenario.icon}</div>
      <h3>${scenario.name}</h3>
      <p>${scenario.tagline}</p>
      <div class="scenario-cta">${scenario.reason}</div>`;
    card.addEventListener("click", () => toast("该方向先做合规评估，本次 Demo 不使用真实未成年人数据。"));
    disabledGrid.appendChild(card);
  });
}

function chooseScenario(id) {
  state.scenario = scenarioById(id);
  const defaults = state.scenario.defaults;
  state.nLog = defaults.n_log;
  state.threshold = defaults.threshold;
  state.common = defaults.common;
  state.parties = defaults.parties;
  $("setupIcon").textContent = state.scenario.icon;
  $("setupScenarioName").textContent = state.scenario.name;
  $("setupTagline").textContent = state.scenario.tagline;
  $("partyALabel").textContent = state.scenario.party_a;
  $("partyBLabel").textContent = state.scenario.party_b;
  $("partyCLabel").textContent = state.scenario.party_c || "C 方";
  $("generatedALabel").textContent = state.scenario.party_a;
  $("generatedBLabel").textContent = state.scenario.party_b;
  $("customALabel").textContent = state.scenario.party_a;
  $("customBLabel").textContent = state.scenario.party_b;
  $("elementLabel").textContent = state.scenario.element;
  $("elementFormatLabel").textContent = state.scenario.element_format;
  $("elementExampleLabel").textContent = state.scenario.element_example;
  $("securityLabel").textContent = state.scenario.security;
  $("dataSourceLabel").textContent = [
    `${state.scenario.party_a}·${state.scenario.data_a}`,
    `${state.scenario.party_b}·${state.scenario.data_b}`,
    state.scenario.party_c ? `${state.scenario.party_c}·${state.scenario.data_c}` : "",
  ].filter(Boolean).join(" / ");
  $("thresholdLabel").textContent = state.scenario.threshold_label;
  $("setupExample").textContent = scenarioExampleText(state.scenario);
  $("nLog").value = state.nLog;
  $("threshold").max = 2 ** state.nLog;
  $("common").max = 2 ** state.nLog;
  $("threshold").value = state.threshold;
  $("common").value = state.common;
  syncPartyMode();
  state.datasetSeed += 1;
  state.customSets = { a: [], b: [], common: [] };
  state.customThreshold = 4;
  if (state.dataMode === "custom") {
    const sample = buildSampleSets();
    $("setAText").value = sample.a.join("\n");
    $("setBText").value = sample.b.join("\n");
    analyzeCustomSets(true);
  }
  syncControls();
  switchScreen("setup");
}

function syncControls() {
  const custom = state.dataMode === "custom";
  const sizeA = custom ? state.customSets.a.length : 2 ** state.nLog;
  const sizeB = custom ? state.customSets.b.length : 2 ** state.nLog;
  const n = custom ? Math.max(1, sizeA, sizeB) : 2 ** state.nLog;
  if (custom) {
    state.customThreshold = Math.max(0, Math.min(n, Math.round(state.customThreshold)));
  } else {
    state.threshold = Math.max(0, Math.min(n, Math.round(state.threshold)));
    state.common = Math.max(0, Math.min(n, Math.round(state.common)));
  }
  const liveLocked = state.mode === "live" && !state.backend.patched;
  if (liveLocked) state.common = state.threshold;
  $("nLog").value = state.nLog;
  $("nLog").disabled = custom;
  $("nSizeGroup").classList.toggle("custom-mode", custom);
  $("threshold").max = n;
  $("common").max = n;
  if (custom) {
    $("threshold").value = state.customThreshold;
    $("thresholdValue").textContent = fmt(state.customThreshold);
    $("nValue").textContent = `A ${fmt(sizeA)} / B ${fmt(sizeB)}`;
    $("commonValue").textContent = fmt(state.customSets.common.length);
  } else {
    $("threshold").value = state.threshold;
    $("common").value = state.common;
    $("nValue").textContent = `2^${state.nLog} = ${fmt(n)}`;
    $("thresholdValue").textContent = fmt(state.threshold);
    $("commonValue").textContent = fmt(state.common);
  }
  $("nHint").textContent = custom
    ? "手动输入模式直接使用 A/B 双方元素；最多 200 条/方，百万级请切回自动生成。"
    : (state.mode === "live"
      ? "现场真实执行建议不超过 2^11（约 8 秒）；2^12 以上会明显变慢，2^20 为论文口径。"
      : (state.nLog > 17
        ? "2^18..2^20 为论文基准；当前真实后端最大支持 2^17。"
        : "当前规模在真实后端可跑范围内（最大 2^17）。"));
  $("thresholdHint").textContent = custom
    ? `${state.scenario ? state.scenario.threshold_label : "门限"}：交集 ≥ k 才输出匹配；当前 A/B 最多 ${fmt(n)} 条。`
    : `${state.scenario ? state.scenario.threshold_label : "门限"}：交集 ≥ k 才输出匹配；否则输出 ⊥。`
      + (state.mode === "live" ? " 真实后端 k 越接近集合规模越慢（离线多项式次数 ∝ k）；现场建议 k ≤ 500。" : "");
  $("common").disabled = custom || liveLocked;
  $("commonGroup").classList.toggle("custom-mode", custom);
  $("commonHint").textContent = custom
    ? "手动模式下由 A/B 输入的交集自动计算，不能手填。"
    : (liveLocked
      ? "真实后端当前把门限同时当作实际交集；要演示拒绝分支，请应用补丁并设置 MYPSI_PATCHED=1。"
      : "仅演示模式可调；真实后端由协议算出，不直接暴露。");
  updateBackendNote();
  if (!custom) renderGeneratedDataset();
}

function renderRunSteps() {
  const list = $("stepList");
  list.innerHTML = "";
  PROTOCOL_STEPS.forEach(([id, name, detail], index) => {
    const li = document.createElement("li");
    li.id = `step-${id}`;
    li.innerHTML = `<span class="bullet">${index + 1}</span><div><strong>${name}</strong><p>${detail}</p></div>`;
    list.appendChild(li);
  });
}

function resetRunSteps() {
  PROTOCOL_STEPS.forEach(([id]) => {
    const li = $(`step-${id}`);
    if (li) li.className = "";
  });
  $("flowDot").classList.add("on");
  $("coach").textContent = "正在准备数据与参数…";
}

function estimateSeconds() {
  const n = state.dataMode === "custom"
    ? Math.max(1, state.customSets.a.length, state.customSets.b.length)
    : 2 ** state.nLog;
  const online = 7.36 * (n / 1048576) ** 0.75;
  return Math.min(9, Math.max(state.dataMode === "custom" ? 0.9 : 1.8, online + 0.7));
}

async function animateProtocol(token) {
  const total = estimateSeconds();
  const weights = [0.30, 0.22, 0.18, 0.16, 0.14];
  for (let index = 0; index < PROTOCOL_STEPS.length; index += 1) {
    if (token !== state.runToken) return false;
    const [id] = PROTOCOL_STEPS[index];
    const li = $(`step-${id}`);
    li.classList.add("active");
    $("coach").textContent = COACH_LINES[id];
    await sleep(total * 1000 * weights[index] * 0.72);
    li.classList.remove("active");
    li.classList.add("done");
  }
  if (token === state.runToken) $("coach").textContent = COACH_LINES.waiting;
  return true;
}

function setStepState(activeIndex) {
  PROTOCOL_STEPS.forEach(([id], index) => {
    const li = $(`step-${id}`);
    if (!li) return;
    li.classList.toggle("done", index < activeIndex);
    li.classList.toggle("active", index === activeIndex);
  });
}

function applyProgress(snapshot) {
  if (!snapshot || !snapshot.active) return;
  const stage = Math.max(0, Math.min(5, Number(snapshot.stage || 0)));
  const index = stage >= 5 ? PROTOCOL_STEPS.length : Math.min(stage, PROTOCOL_STEPS.length - 1);
  setStepState(index);
  const step = PROTOCOL_STEPS[Math.min(index, PROTOCOL_STEPS.length - 1)];
  $("coach").textContent = snapshot.message || (step ? COACH_LINES[step[0]] : COACH_LINES.waiting);
  $("flowDot").classList.add("on");

  const percent = Number.isFinite(Number(snapshot.percent))
    ? Math.max(0, Math.min(100, Number(snapshot.percent)))
    : Math.min(100, stage * 20);
  $("runProgress").classList.remove("hidden");
  $("runProgressBar").style.width = `${percent}%`;
  $("runProgress").setAttribute("aria-valuenow", String(Math.round(percent)));

  if (snapshot.offline_s != null) $("tOffline").textContent = `${Number(snapshot.offline_s).toFixed(2)}s`;
  if (snapshot.share_s != null) $("tShare").textContent = `${Number(snapshot.share_s).toFixed(2)}s`;
  if (snapshot.intersection_s != null) $("tIntersect").textContent = `${Number(snapshot.intersection_s).toFixed(2)}s`;
  $("stageTimes").classList.remove("hidden");

  const shown = Math.min(5, Math.max(1, stage >= 5 ? 5 : stage + 1));
  const elapsed = snapshot.elapsed == null ? "" : ` · 后端 ${Number(snapshot.elapsed).toFixed(1)}s`;
  $("runMeta").textContent = `${state.runMetaBase} · 阶段 ${shown}/5${elapsed}`;
}

function startProgressPolling(token) {
  stopProgressPolling();
  state.progressTimer = setInterval(async () => {
    if (token !== state.runToken) return;
    try {
      const response = await fetch("/api/progress", { cache: "no-store" });
      if (!response.ok) return;
      const snapshot = await response.json();
      if (token !== state.runToken) return;
      applyProgress(snapshot);
    } catch (_) {
      /* a dropped poll is not fatal; the run request still carries the result */
    }
  }, 700);
}

function stopProgressPolling() {
  if (state.progressTimer) clearInterval(state.progressTimer);
  state.progressTimer = null;
}

function hideRunError() {
  state.runFailed = false;
  $("runError").classList.add("hidden");
  $("cancelBtn").classList.remove("hidden");
}

function showRunError(message, detail) {
  state.runFailed = true;
  const text = String(message || "未知错误").trim();
  const trace = String(detail || "").trim();
  state.lastRunError = { message: text, detail: trace };
  $("runErrorMessage").textContent = text;
  $("runErrorDetail").textContent = trace;
  $("runErrorDetail").classList.add("hidden");
  $("runToggleDetailBtn").classList.toggle("hidden", !trace);
  $("runToggleDetailBtn").textContent = "显示原始错误";
  $("runError").classList.remove("hidden");
  $("cancelBtn").classList.add("hidden");
  $("coach").textContent = "运行已中断，错误信息停在下方，可直接截图汇报。";
  $("flowDot").classList.remove("on");
}

async function cancelRun() {
  const wasLive = state.mode === "live" && state.running;
  state.runToken += 1;
  clearInterval(state.timer);
  state.timer = null;
  stopProgressPolling();
  state.running = false;
  hideRunError();
  if (wasLive) {
    try {
      const response = await fetch("/api/cancel", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: "{}",
      });
      const data = await response.json();
      toast(data.cancelled
        ? `已终止后端进程${data.killed ? `（${data.killed} 个）` : ""}`
        : (data.message || "任务已结束"));
    } catch (error) {
      toast(`取消请求失败：${error.message}`);
    }
  } else {
    toast("已取消本次演示");
  }
  switchScreen("setup");
}

async function startRun() {
  if (!state.scenario) {
    toast("先选一个场景");
    return;
  }
  if (state.running) {
    toast("已有任务在运行，请先取消或等待完成");
    switchScreen("running");
    return;
  }
  if (state.mode === "live" && !state.backend.exists) {
    toast("未配置真实后端，已切回演示模式");
    setMode("mock");
  }
  if (state.mode === "live" && state.nLog > 17) {
    state.nLog = 17;
    syncControls();
    toast("真实后端当前最大支持 2^17，已自动切到 131,072");
  }
  const custom = state.dataMode === "custom";
  if (custom && !analyzeCustomSets(true)) {
    toast("先填写 A/B 两方数据并计算交集");
    return;
  }
  const sizeA = custom ? state.customSets.a.length : 2 ** state.nLog;
  const sizeB = custom ? state.customSets.b.length : 2 ** state.nLog;
  const runN = custom ? Math.max(1, sizeA, sizeB) : 2 ** state.nLog;
  const runThreshold = custom ? state.customThreshold : state.threshold;
  const runCommon = custom ? state.customSets.common.length : state.common;
  const runLog = custom ? Math.max(0, Math.ceil(Math.log2(runN))) : state.nLog;
  state.running = true;
  state.lastResult = null;
  state.runToken += 1;
  const token = state.runToken;
  stopProgressPolling();
  hideRunError();
  resetRunSteps();
  setStepState(0);
  $("runScenario").textContent = state.scenario.name;
  $("flowA").textContent = state.scenario.party_a;
  $("flowB").textContent = state.scenario.party_b;
  $("runTimer").textContent = "0.00s";
  const live = state.mode === "live";
  $("privacyBanner").textContent = custom ? CUSTOM_PRIVACY_BANNER : DEFAULT_PRIVACY_BANNER;
  $("runProgress").classList.toggle("hidden", !live);
  $("runProgressBar").style.width = "0%";
  $("runProgress").setAttribute("aria-valuenow", "0");
  $("stageTimes").classList.toggle("hidden", !live);
  ["tOffline", "tShare", "tIntersect"].forEach((id) => { $(id).textContent = "—"; });
  const sizeMeta = custom
    ? `A ${fmt(sizeA)} / B ${fmt(sizeB)}（手动）`
    : `2^${state.nLog}（${fmt(2 ** state.nLog)}）`;
  state.runMetaBase = `${live ? "真实后端" : "演示模拟"} · ${sizeMeta} · 门限 k=${fmt(runThreshold)} · ${state.parties} 方`;
  $("runMeta").textContent = state.runMetaBase;
  $("cancelBtn").textContent = live ? "取消（真实终止后端）" : "取消（回到参数）";
  switchScreen("running");

  const started = performance.now();
  state.timer = setInterval(() => {
    $("runTimer").textContent = `${((performance.now() - started) / 1000).toFixed(2)}s`;
  }, 50);

  const payload = {
    scenario: state.scenario.id,
    n_log: runLog,
    threshold: runThreshold,
    common: runCommon,
    data_mode: state.dataMode,
    parties: state.parties,
    mode: state.mode,
    security: "semi-honest",
  };
  if (custom) {
    payload.set_a = state.customSets.a;
    payload.set_b = state.customSets.b;
  }

  try {
    const request = fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    let response;
    if (state.mode === "live") {
      startProgressPolling(token);
      response = await request;
    } else {
      [response] = await Promise.all([request, animateProtocol(token)]);
    }
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    if (token !== state.runToken) return;
    clearInterval(state.timer);
    state.timer = null;
    stopProgressPolling();
    setStepState(PROTOCOL_STEPS.length);
    if (state.mode === "live") {
      $("runProgressBar").style.width = "100%";
      $("runProgress").setAttribute("aria-valuenow", "100");
    }
    state.running = false;
    state.lastResult = data;
    renderResult(data);
    switchScreen("result");
  } catch (error) {
    if (token !== state.runToken) return;
    clearInterval(state.timer);
    state.timer = null;
    stopProgressPolling();
    state.running = false;
    showRunError(error.message || String(error), error.stack || "");
    switchScreen("running");
  }
}

function metric(label, value) {
  return `<div class="metric"><span>${label}</span><strong>${value}</strong></div>`;
}

function renderResult(result) {
  const accept = result.decision === "accept";
  const custom = result.data_mode === "custom";
  const scenario = scenarioById(result.scenario) || state.scenario || {};
  const thresholdLabel = scenario.threshold_label || "门限 k";
  const scenarioName = scenario.name || result.scenario_name || result.scenario || "本次场景";
  const roleText = scenarioRoleText(scenario);
  const card = $("decisionCard");
  card.classList.toggle("accept", accept);
  card.classList.toggle("reject", !accept);
  $("decisionBadge").textContent = accept ? "达到门限" : "未达到门限";
  $("resultBig").textContent = accept ? fmt(result.intersection_size) : "⊥";
  $("resultSub").textContent = accept
    ? `≥ ${thresholdLabel} ${fmt(result.threshold)}，输出交集`
    : `< ${thresholdLabel} ${fmt(result.threshold)}，不输出交集`;
  $("resultExplain").textContent = custom
    ? (accept
      ? `${scenarioName}（${roleText || "A/B"}）：本地演示按 A/B 两方输入计算交集；真实 PSI 应由协议计算，原始集合不出域。`
      : `${scenarioName}（${roleText || "A/B"}）：本地演示的交集为 ${fmt(result.actual_intersection)} 条，低于门槛；真实门限 PSI 不会输出具体交集。`)
    : (accept
      ? `${scenarioName}（${roleText || "A/B"}）：${scenario.real_note || "各参与方得到交集规模与业务匹配"}；原始集合始终不出域。`
      : `${scenarioName}（${roleText || "A/B"}）：${scenario.real_note || "各参与方只知道共同元素不够"}；未达到门槛时不会得到具体交集，这是门限 PSI 与普通 PSI 的关键区别。`);

  const m = result.metrics || {};
  $("metricGrid").innerHTML = (custom ? [
    metric("本地求交耗时", formatLocalDuration(m.total_s)),
    metric("A / B 规模", `${fmt(result.size_a)} / ${fmt(result.size_b)}`),
    metric("实际交集", `${fmt(result.actual_intersection)} 条`),
    metric("门限 k", `${fmt(result.threshold)} 条`),
    metric("参与方", `${result.parties} 方`),
    metric("安全模型", "本地演示"),
    metric("通信量（未实测）", "本地计算"),
  ] : [
    metric("端到端总耗时", `${Number(m.total_s || 0).toFixed(2)}s`),
    metric("在线阶段", `${Number(m.online_s || 0).toFixed(2)}s`),
    metric("交集阶段", `${Number(m.intersection_s || 0).toFixed(2)}s`),
    metric("通信量", `${Number(m.comm_mb || 0).toFixed(1)} MB`),
    metric("A / B 规模", `${fmt(result.size_a == null ? result.n : result.size_a)} / ${fmt(result.size_b == null ? result.n : result.size_b)}`),
    metric("参与方", `${result.parties} 方`),
    metric("安全模型", m.security === "semi-honest" ? "半诚实" : (m.security === "demo-only" ? "本地演示" : (m.security || "—"))),
  ]).join("");

  $("actionCard").textContent = (accept ? scenario.action_accept : scenario.action_reject) || "";
  $("resultSource").textContent = custom
    ? "本地集合求交（演示）"
    : (result.mode === "live" ? "真实后端日志解析" : "演示模拟（论文锚定）");
  if (custom) {
    const preview = (result.intersection_preview || []).map(String).join("、");
    const previewNote = preview ? `交集样例：${preview}` : "没有共同元素";
    $("demoNote").textContent = `手动输入模式：A 方 ${fmt(result.size_a)} 条，B 方 ${fmt(result.size_b)} 条，实际交集 ${fmt(result.actual_intersection)} 条。${previewNote}。${accept ? "" : "门限未达到，真实 TPSI 不会输出交集；这里显示的是本地演示后台值。"} ${result.note || ""}`;
  } else {
    $("demoNote").textContent = result.mode === "live"
      ? `真实后端输出仅保留阶段耗时与交集规模，未展示份额/密钥。${result.note || ""}`
      : `演示说明：后台模拟实际交集 c=${fmt(result.actual_intersection)}，仅用于演示“改门限翻转结果”；协议本身在未达到门限时不会输出这个值。${result.note || ""}`;
  }
  if (result.mode === "live") {
    const paper = PAPER_IV[result.n_log];
    const wallText = result.wall_s != null
      ? `${Number(result.wall_s).toFixed(2)}s`
      : `${Number(m.total_s || 0).toFixed(2)}s`;
    if (paper) {
      $("demoNote").textContent += Number(result.threshold) === paper.t
        ? ` 论文 Table IV 同配置（N=3, LAN, t=0.6n=${fmt(paper.t)}）为 ${paper.s.toFixed(2)}s；本次墙钟 ${wallText}，协议总 ${Number(m.total_s || 0).toFixed(2)}s。`
        : ` 论文 Table IV 在 n=${fmt(paper.n)}、t=0.6n=${fmt(paper.t)} 时为 ${paper.s.toFixed(2)}s；本次 k=${fmt(result.threshold)} 不是同一门限配置，不能直接对比。`;
    } else {
      $("demoNote").textContent += " 论文 Table IV 只覆盖 n=512..16384；2^20 的 7.36s 是 CCS '23 pTPSI 口径，不是这篇论文的数据。";
    }
  }
}

function replay() {
  if (!state.lastResult) return;
  const result = state.lastResult;
  if (result.mode === "live" && result.actual_intersection == null) {
    toast("当前二进制未拆分“实际交集/门限”，改门限不会翻转；请先应用 patches/ 补丁");
    switchScreen("setup");
    return;
  }
  const custom = result.data_mode === "custom";
  const maxSize = custom
    ? Math.max(1, state.customSets.a.length, state.customSets.b.length)
    : 2 ** state.nLog;
  const base = Number(result.actual_intersection || (custom ? 0 : state.common));
  let next;
  if (result.decision === "accept") {
    next = Math.round(base) + Math.max(1, Math.round(base * 0.1));
    if (next > maxSize) {
      toast(custom
        ? "实际交集已达两方集合上限，调高门限也无法翻转；可减少 A/B 的共同元素后再试"
        : "门限已到本场景上限，无法再调高；可减少“实际交集”后再试");
      switchScreen("setup");
      return;
    }
    next = Math.min(maxSize, next);
    if (custom) state.customThreshold = next;
    else state.threshold = next;
    toast(`门限已调到 ${fmt(next)}，再跑一次看“未达到”`);
  } else {
    next = Math.max(0, Math.round(base * 0.85));
    if (custom) state.customThreshold = next;
    else state.threshold = next;
    toast(`门限已调到 ${fmt(next)}，再跑一次看“达到”`);
  }
  syncControls();
  switchScreen("setup");
}

async function loadBench() {
  if (state.benchData) return state.benchData;
  try {
    const response = await fetch("/api/bench", { cache: "no-store" });
    state.benchData = await response.json();
  } catch (error) {
    state.benchData = { available: false, cases: [], error: error.message };
  }
  return state.benchData;
}

function benchSeconds(item) {
  return Number(item.total_s != null ? item.total_s : (item.wall_s || 0));
}

async function renderBenchmark() {
  const bench = await loadBench();
  const rows = [
    { name: "旧 TPSI [20]", seconds: 778.9, note: "2^11 集合（论文）" },
    { name: "旧拼车 TPSI [18]", seconds: 5627, note: "2^12 集合（论文）" },
    { name: "旧方案 [26]", seconds: 222.3, note: "100 元素（论文）" },
    { name: "2023 pTPSI", seconds: 7.36, note: "2^20，k=8000，32 线程（论文）", highlight: true },
  ];
  (bench.cases || []).forEach((item) => {
    const seconds = benchSeconds(item);
    if (!seconds) return;
    const comm = item.comm_mb == null ? "" : ` · ${Number(item.comm_mb).toFixed(1)}MB`;
    rows.push({
      name: `本机 2^${item.n}`,
      seconds,
      note: `k=${fmt(item.k)}${comm}`,
      measured: true,
    });
  });
  const last = state.lastResult;
  if (last) {
    rows.push({
      name: "本次演示",
      seconds: Number(last.metrics.total_s || 0),
      note: `${fmt(last.n)} 元素`,
      measured: last.mode === "live",
      highlight: last.mode !== "live",
    });
  }
  const max = Math.max(...rows.map((row) => row.seconds), 1);
  const width = (seconds) => `${Math.max(2, (Math.log10(seconds + 1) / Math.log10(max + 1)) * 100)}%`;
  $("benchChart").innerHTML = rows.map((row) => `
    <div class="bench-row ${row.highlight ? "highlight" : ""} ${row.measured ? "measured" : ""}">
      <span>${escapeHtml(row.name)}</span>
      <div class="bench-track"><div class="bench-fill" style="width:${width(row.seconds)}"></div></div>
      <b>${row.seconds < 10 ? row.seconds.toFixed(2) : fmt(Math.round(row.seconds))}s</b>
    </div>`).join("");

  const back = $("benchBackBtn");
  if (back) back.textContent = state.lastResult ? "← 返回结果" : (state.scenario ? "← 返回参数" : "← 返回场景");

  const meta = $("benchMeta");
  if (bench.available) {
    meta.classList.remove("warn");
    const cpu = bench.cpu ? ` · ${escapeHtml(bench.cpu)}` : "";
    const when = bench.generated_at ? ` · 生成于 ${escapeHtml(bench.generated_at)}` : "";
    const system = [bench.os, bench.kernel].filter(Boolean).map((value) => escapeHtml(value)).join(" / ");
    const systemPart = system ? ` · ${system}` : "";
    const patchPart = bench.patched == null ? "" : ` · ${bench.patched ? "已打补丁" : "原版二进制"}`;
    meta.innerHTML = `本机实测 ${bench.cases.length} 组：${escapeHtml(bench.host || "本机")} · ${bench.nproc || "?"} 核${cpu} · ${bench.mem_gb || "?"} GB${systemPart}${when}${patchPart}；串行单次运行，与论文 32 线程不可直接横比。`;
  } else {
    meta.classList.add("warn");
    meta.textContent = "尚无本机实测：在 Linux 上运行 bash demo/scripts/bootstrap_linux.sh --src <源码> --bench 生成 data/bench.json，刷新本页即可接入；当前图表只有论文数字。";
  }

  const table = [
    ["2023 pTPSI", "2 方，2^20，k=8000", "在线 7.36s（半诚实）/ 9.74s（恶意）", "CCS '23 表 7.7/7.8"],
    ["旧 TPSI [20]", "2^11", "778.9s", "论文 1 引言"],
    ["旧拼车 [18]", "2^12", "5627s", "论文 1 引言"],
    ["旧方案 [26]", "100", "222.306s", "论文 1 引言"],
    ["论文 4 IoV", "N≤10，n≤16384", "通信 125MB（1024/10 方）", "TVT '25"],
  ];
  (bench.cases || []).forEach((item) => {
    const total = item.total_s == null ? "—" : `${Number(item.total_s).toFixed(2)}s`;
    const comm = item.comm_mb == null ? "—" : `${Number(item.comm_mb).toFixed(1)} MB`;
    const inter = item.intersection == null ? "—" : fmt(item.intersection);
    table.push([
      "本机实测",
      `2^${item.n}，k=${item.k}`,
      `墙钟 ${Number(item.wall_s || 0).toFixed(2)}s · 协议总 ${total} · ${comm} · 交集 ${inter}`,
      escapeHtml(bench.host || "本机"),
    ]);
  });
  $("benchTable").innerHTML = `<thead><tr><th>工作</th><th>设置</th><th>结果</th><th>出处</th></tr></thead><tbody>${table.map((row) => `<tr>${row.map((cell) => `<td>${cell}</td>`).join("")}</tr>`).join("")}</tbody>`;
}

function benchmarkSummary(bench) {
  const lines = ["【PSI 性能页摘要】"];
  lines.push("论文口径：2^20 / k=8000，在线 7.36s（半诚实）、9.74s（恶意）；CCS '23 表 7.7/7.8，LAN、32 线程。");
  if (bench && bench.available) {
    const cpu = bench.cpu ? ` · ${bench.cpu}` : "";
    lines.push(`本机：${bench.host || "本机"} · ${bench.nproc || "?"} 核 · ${bench.mem_gb || "?"} GB${cpu}`);
    if (bench.generated_at) lines.push(`生成时间：${bench.generated_at}`);
    const system = [bench.os, bench.kernel].filter(Boolean).join(" / ");
    if (system) lines.push(`系统：${system}`);
    if (bench.patched != null) lines.push(`补丁：${bench.patched ? "已打补丁（门限/实际交集分离）" : "原版二进制"}`);
    (bench.cases || []).forEach((item) => {
      const total = item.total_s == null ? "—" : `${Number(item.total_s).toFixed(2)}s`;
      const comm = item.comm_mb == null ? "—" : `${Number(item.comm_mb).toFixed(1)} MB`;
      const inter = item.intersection == null ? "—" : fmt(item.intersection);
      const k = item.k == null ? "?" : fmt(item.k);
      lines.push(`- 2^${item.n}，k=${k}：墙钟 ${Number(item.wall_s || 0).toFixed(2)}s，协议总 ${total}，通信 ${comm}，交集 ${inter}`);
    });
  } else {
    lines.push("本机实测：尚无（未生成 demo/data/bench.json）。");
  }
  const last = state.lastResult;
  if (last) {
    lines.push(`本次演示：${last.mode === "live" ? "真实后端" : "演示模拟"}，2^${last.n_log}，门限 ${fmt(last.threshold)}，总耗时 ${Number((last.metrics || {}).total_s || 0).toFixed(2)}s。`);
  }
  lines.push("说明：本机为串行单次运行，与论文 32 线程 LAN 设置不可直接横比；论文数字只作数量级参考。");
  return lines.join("\n");
}

async function downloadBench() {
  const bench = await loadBench();
  if (!bench.available) {
    toast("尚无本机实测；先在 Linux 上生成 demo/data/bench.json");
    return;
  }
  const payload = {
    app: "PSI Lab Demo",
    exported_at: new Date().toISOString(),
    summary: benchmarkSummary(bench),
    bench,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: "application/json;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = "psi-bench.json";
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1500);
  toast("已下载本机实测 bench.json");
}

function conclusionText(result) {
  const accept = result.decision === "accept";
  const custom = result.data_mode === "custom";
  const m = result.metrics || {};
  const scenario = scenarioById(result.scenario) || state.scenario || {};
  const roles = scenarioRoleText(scenario);
  const thresholdLabel = scenario.threshold_label || "门限 k";
  const lines = [
    `【PSI 演示结论】${result.scenario_name || result.scenario}`,
    `场景：${scenario.name || result.scenario_name || result.scenario || "—"}；角色：${roles || "A/B"}`,
    `门限含义：${thresholdLabel}`,
    custom
      ? "模式：本地演示（Python 集合求交，非密码学协议）"
      : `模式：${result.mode === "live" ? "真实后端（Linux ELF）" : "演示模拟（CCS '23 论文锚定）"}`,
    custom
      ? `规模：A 方 ${fmt(result.size_a)} 条 / B 方 ${fmt(result.size_b)} 条；实际交集 ${fmt(result.actual_intersection)} 条；门限 k = ${fmt(result.threshold)}`
      : `规模：2^${result.n_log} = ${fmt(result.n)}；门限 k = ${fmt(result.threshold)}`,
    accept
      ? `结果：达到门限，交集规模 ${fmt(result.intersection_size)}，输出匹配结果；${scenario.action_accept || ""}`
      : `结果：未达到门限（⊥），各参与方不获得具体交集；${scenario.action_reject || ""}`,
    custom
      ? `耗时：本地求交 ${formatLocalDuration(m.total_s)}；通信量未实测（无网络交互）。`
      : `耗时：总 ${Number(m.total_s || 0).toFixed(2)}s（在线 ${Number(m.online_s || 0).toFixed(2)}s，交集阶段 ${Number(m.intersection_s || 0).toFixed(2)}s）；通信 ${Number(m.comm_mb || 0).toFixed(1)} MB。`,
    `run_id：${result.run_id}`,
  ];
  if (custom) {
    lines.push("说明：真实 C++ PSI 尚未接入任意集合；手动模式仅用于讲清 A/B 交集与门限判断。");
  } else {
    lines.push("说明：原始集合不出域；演示数据为合成数据，未使用真实个人信息。");
  }
  return lines.join("\n");
}

function exportResult() {
  const result = state.lastResult;
  if (!result) return;
  const payload = {
    app: "PSI Lab Demo",
    exported_at: new Date().toISOString(),
    conclusion: conclusionText(result),
    result,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: "application/json;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `psi-run-${result.run_id}.json`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1500);
  toast("已导出本次运行的 JSON");
}

async function copyConclusion() {
  const result = state.lastResult;
  if (!result) return;
  const ok = await copyText(conclusionText(result));
  toast(ok ? "结论已复制到剪贴板" : "复制失败，请手动选择结果文字");
}

function diagStatusText(status) {
  return ({ ok: "通过", warn: "注意", fail: "失败" })[status] || status;
}

function diagSummaryText() {
  const data = state.selfcheck;
  if (!data) return "尚未完成部署自检";
  const server = data.server || {};
  const lines = [
    `【PSI Demo 部署自检】${data.overall === "ok" ? "环境就绪" : data.overall === "warn" ? "可以演示，但有提醒" : "存在阻断项"}`,
    `版本：v${server.version || "?"}；启动：${server.started_at || "?"}；已运行 ${server.uptime_s == null ? "?" : server.uptime_s}s`,
    `服务：${(server.urls || []).join(" / ") || "?"}`,
    `Python：${server.python || "?"}；系统：${server.platform || "?"}`,
    `前端资源时间：${server.asset_stamp || "未知"}`,
    `检查时间：${data.checked_at || "?"}`,
  ];
  (data.checks || []).forEach((item) => {
    lines.push(`- [${diagStatusText(item.status)}] ${item.label}：${item.detail}`);
  });
  return lines.join("\n");
}

function renderDiagnostics(data) {
  const server = data.server || {};
  const summary = $("diagSummary");
  summary.className = `diag-summary ${data.overall || ""}`;
  const overallText = data.overall === "ok" ? "环境就绪" : data.overall === "warn" ? "可以演示，但有提醒" : "存在阻断项";
  const uptime = server.uptime_s == null ? "?" : server.uptime_s;
  summary.innerHTML = `<strong>${overallText}</strong><span>检查时间 ${escapeHtml(data.checked_at || "—")} · 版本 v${escapeHtml(server.version || "?")} · 已运行 ${escapeHtml(String(uptime))}s</span>`;
  const urls = (server.urls || []).map((url) => `<code>${escapeHtml(url)}</code>`).join(" · ");
  $("diagServer").innerHTML = `
    <div>服务地址：${urls || "—"}</div>
    <div>启动时间：${escapeHtml(server.started_at || "—")} · Python ${escapeHtml(server.python || "?")}</div>
    <div>系统：${escapeHtml(server.platform || "—")}</div>
    <div>前端资源：${escapeHtml(server.asset_stamp || "未知")}</div>`;
  $("diagList").innerHTML = (data.checks || []).map((item) => `
    <div class="diag-item ${escapeHtml(item.status)}">
      <span class="tag">${escapeHtml(diagStatusText(item.status))}</span>
      <div><strong>${escapeHtml(item.label)}</strong><p>${escapeHtml(item.detail)}</p></div>
    </div>`).join("");
}

async function openDiagnostics() {
  state.selfcheck = null;
  $("diagModal").classList.remove("hidden");
  document.body.classList.add("modal-open");
  $("diagSummary").className = "diag-summary";
  $("diagSummary").textContent = "正在检查…";
  $("diagServer").textContent = "";
  $("diagList").innerHTML = "";
  try {
    const response = await fetch("/api/selfcheck", { cache: "no-store" });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    state.selfcheck = data;
    if (data.server) state.version = data.server;
    renderDiagnostics(data);
  } catch (error) {
    $("diagSummary").className = "diag-summary fail";
    $("diagSummary").textContent = `自检请求失败：${error.message}`;
  }
}

function closeDiagnostics() {
  $("diagModal").classList.add("hidden");
  document.body.classList.remove("modal-open");
}

function bindEvents() {
  document.querySelectorAll("#stepNav button").forEach((button) => {
    button.addEventListener("click", () => {
      const target = button.dataset.target;
      if (target === "running") return;
      if (target === "result" && !state.lastResult) return;
      if (target === "setup" && !state.scenario) return;
      if (target === "benchmark") renderBenchmark();
      switchScreen(target);
    });
  });
  document.querySelectorAll("[data-target]").forEach((button) => {
    if (button.closest("#stepNav")) return;
    button.addEventListener("click", () => switchScreen(button.dataset.target));
  });

  $("nLog").addEventListener("input", (event) => {
    const oldN = 2 ** state.nLog;
    const newLog = Number(event.target.value);
    const newN = 2 ** newLog;
    const ratio = newN / oldN;
    state.threshold = Math.round(state.threshold * ratio);
    state.common = Math.round(state.common * ratio);
    state.nLog = newLog;
    syncControls();
  });
  $("threshold").addEventListener("input", (event) => {
    const value = Number(event.target.value);
    if (state.dataMode === "custom") state.customThreshold = value;
    else state.threshold = value;
    syncControls();
  });
  $("common").addEventListener("input", (event) => {
    state.common = Number(event.target.value);
    syncControls();
  });
  $("parties").addEventListener("change", (event) => {
    const value = Number(event.target.value);
    if (state.mode === "mock" && value !== 2) {
      event.target.value = "2";
      state.parties = 2;
      toast("三方 Qin-3 协议请先切换到“真实后端”");
      return;
    }
    state.parties = value;
    syncPartyCards();
  });
  document.querySelectorAll("#dataModeToggle button").forEach((button) => {
    button.addEventListener("click", () => setDataMode(button.dataset.dataMode));
  });
  $("regenerateDataBtn").addEventListener("click", () => {
    state.datasetSeed += 1;
    renderGeneratedDataset();
    toast("已重新生成样例数据");
  });
  ["setAText", "setBText"].forEach((id) => {
    $(id).addEventListener("input", updateCustomCounts);
  });
  $("sampleSetsBtn").addEventListener("click", () => {
    const sample = buildSampleSets();
    $("setAText").value = sample.a.join("\n");
    $("setBText").value = sample.b.join("\n");
    analyzeCustomSets();
  });
  $("clearSetsBtn").addEventListener("click", () => {
    $("setAText").value = "";
    $("setBText").value = "";
    state.customSets = { a: [], b: [], common: [] };
    $("customResult").classList.add("hidden");
    updateCustomCounts();
    syncControls();
  });
  $("analyzeSetsBtn").addEventListener("click", () => analyzeCustomSets());
  document.querySelectorAll("#modeToggle button").forEach((button) => {
    button.addEventListener("click", () => {
      if (button.dataset.mode === "live" && !state.backend.exists) {
        toast("未配置 MYPSI_BIN；先看演示模式，Linux 环境就绪后再切实后端");
      }
      setMode(button.dataset.mode);
    });
  });
  $("startBtn").addEventListener("click", startRun);
  $("cancelBtn").addEventListener("click", cancelRun);
  $("runRetryBtn").addEventListener("click", startRun);
  $("runBackBtn").addEventListener("click", () => {
    hideRunError();
    switchScreen("setup");
  });
  $("runToggleDetailBtn").addEventListener("click", () => {
    const detail = $("runErrorDetail");
    detail.classList.toggle("hidden");
    $("runToggleDetailBtn").textContent = detail.classList.contains("hidden") ? "显示原始错误" : "收起原始错误";
  });
  $("runCopyErrorBtn").addEventListener("click", async () => {
    if (!state.lastRunError) return;
    const ok = await copyText(`${state.lastRunError.message}\n${state.lastRunError.detail}`.trim());
    toast(ok ? "错误信息已复制" : "复制失败，请手动选择");
  });
  $("exportBtn").addEventListener("click", exportResult);
  $("copyBtn").addEventListener("click", copyConclusion);
  $("replayBtn").addEventListener("click", replay);
  $("changeScenarioBtn").addEventListener("click", () => switchScreen("scenario"));
  $("benchmarkBtn").addEventListener("click", async () => {
    await renderBenchmark();
    switchScreen("benchmark");
  });
  $("benchBackBtn").addEventListener("click", () => {
    if (state.lastResult) {
      switchScreen("result");
    } else if (state.scenario) {
      switchScreen("setup");
    } else {
      switchScreen("scenario");
    }
  });
  $("benchCopyBtn").addEventListener("click", async () => {
    const bench = await loadBench();
    const ok = await copyText(benchmarkSummary(bench));
    toast(ok ? "实测摘要已复制" : "复制失败，请手动选择");
  });
  $("benchDownloadBtn").addEventListener("click", downloadBench);
  $("diagBtn").addEventListener("click", openDiagnostics);
  $("diagCloseBtn").addEventListener("click", closeDiagnostics);
  $("diagRefreshBtn").addEventListener("click", openDiagnostics);
  $("diagCopyBtn").addEventListener("click", async () => {
    const ok = await copyText(diagSummaryText());
    toast(ok ? "自检摘要已复制" : "复制失败，请手动选择");
  });
  document.querySelectorAll("[data-close-diag]").forEach((node) => node.addEventListener("click", closeDiagnostics));
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !$("diagModal").classList.contains("hidden")) closeDiagnostics();
  });
}

async function boot() {
  renderRunSteps();
  bindEvents();
  try {
    const response = await fetch("/api/config");
    const config = await response.json();
    state.config = config;
    state.backend = config.backend;
    try {
      const versionResponse = await fetch("/api/version", { cache: "no-store" });
      if (versionResponse.ok) state.version = await versionResponse.json();
    } catch (_) {
      /* older server without /api/version */
    }
    renderScenarios();
    setMode("mock");
    const versionTag = state.version && state.version.version ? ` · v${state.version.version}` : "";
    $("footerBackend").textContent = `backend: ${config.backend.exists ? "live ready" : "mock only"}${versionTag}`;
    if (state.version && state.version.urls) $("footerBackend").title = state.version.urls.join(" / ");
    if (!config.backend.exists) {
      $("backendNote").classList.add("warn");
      $("backendNote").textContent = "当前未配置真实后端，使用演示模式；Linux 上设置 MYPSI_BIN 后可切换真实后端。";
    }
    const params = new URLSearchParams(window.location.search);
    const autoScenario = params.get("scenario");
    const autoScreen = params.get("screen");
    if (params.get("mode") === "live" && config.backend.exists) setMode("live");
    if (autoScenario && scenarioById(autoScenario)) {
      chooseScenario(autoScenario);
      if (params.get("threshold")) state.threshold = Number(params.get("threshold"));
      if (params.get("common")) state.common = Number(params.get("common"));
      syncControls();
    }
    if (autoScreen === "running" || autoScreen === "result") {
      startRun();
    } else if (autoScreen === "benchmark") {
      await renderBenchmark();
      switchScreen("benchmark");
    } else if (autoScreen === "setup" && autoScenario) {
      switchScreen("setup");
    }
  } catch (error) {
    toast(`加载配置失败：${error.message}`);
  }
}

boot();
