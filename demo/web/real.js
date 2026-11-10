const $ = (id) => document.getElementById(id);
const fmt = (n) => Number(n).toLocaleString("zh-CN");
const MAX_ITEMS = 131072;
const MAX_FILE_BYTES = 12 * 1024 * 1024;

const state = {
  files: { a: null, b: null, c: null },
  busy: false,
  token: 0,
  timer: null,
  poller: null,
  result: null,
  backend: { exists: false, patched: false },
  scenarios: [],
  scenario: null,
  sampleBusy: false,
  sampleToken: 0,
};

function toast(message) {
  const el = $("toast");
  if (!el) return;
  el.textContent = message;
  el.classList.add("show");
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => el.classList.remove("show"), 3200);
}

function fail(message, detail) {
  state.busy = false;
  $("runBtn").disabled = false;
  $("setupPanel").classList.remove("hidden");
  $("progressPanel").classList.add("hidden");
  $("errorPanel").classList.remove("hidden");
  $("errorMessage").textContent = message;
  $("errorDetail").textContent = detail || "";
  $("errorDetail").classList.add("hidden");
  $("errorToggleBtn").classList.toggle("hidden", !detail);
  $("errorToggleBtn").textContent = "显示原始错误";
}

function scenarioById(id) {
  return state.scenarios.find((item) => item.id === id);
}

function scenarioRoleText(scenario) {
  if (!scenario) return "A / B / C";
  return `A ${scenario.party_a} / B ${scenario.party_b} / C ${scenario.party_c}`;
}

function scenarioExampleText(scenario) {
  if (!scenario || !scenario.example) return "";
  const example = scenario.example;
  return `业务示例：${scenario.data_a} ${fmt(example.size_a)} / ${scenario.data_b} ${fmt(example.size_b)} / ${scenario.data_c} ${fmt(example.size_c)} → 共同 ${fmt(example.intersection)}；${scenario.threshold_label} = ${fmt(example.threshold)} 时通过。示例为合成数据，不代表本机实测。`;
}

function renderScenarioTabs() {
  const tabs = $("scenarioTabs");
  tabs.innerHTML = "";
  state.scenarios.forEach((scenario) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "scenario-tab";
    button.dataset.scenario = scenario.id;
    button.textContent = scenario.name;
    button.addEventListener("click", () => applyScenario(scenario.id));
    tabs.appendChild(button);
  });
}

function applyScenario(id, updateUrl = true) {
  const scenario = scenarioById(id) || state.scenarios[0];
  if (!scenario) return;
  const changed = !state.scenario || state.scenario.id !== scenario.id;
  state.scenario = scenario;
  document.querySelectorAll("#scenarioTabs .scenario-tab").forEach((button) => {
    button.classList.toggle("active", button.dataset.scenario === scenario.id);
  });
  $("roleLabelA").textContent = scenario.party_a;
  $("roleLabelB").textContent = scenario.party_b;
  $("roleLabelC").textContent = scenario.party_c;
  $("sourceLabelA").textContent = scenario.data_a;
  $("sourceLabelB").textContent = scenario.data_b;
  $("sourceLabelC").textContent = scenario.data_c;
  $("thresholdLabel").textContent = scenario.threshold_label;
  $("scenarioRealNote").textContent = `${scenario.tagline} ${scenario.real_note}`;
  $("sampleLinkA").href = `/static/samples/${scenario.id}_A.txt`;
  $("sampleLinkB").href = `/static/samples/${scenario.id}_B.txt`;
  $("sampleLinkC").href = `/static/samples/${scenario.id}_C.txt`;
  $("sampleLinkA").textContent = `下载 ${scenario.party_a} 样例`;
  $("sampleLinkB").textContent = `下载 ${scenario.party_b} 样例`;
  $("sampleLinkC").textContent = `下载 ${scenario.party_c} 样例`;
  document.title = `PSI Lab · ${scenario.name} · 真实数据三方求交`;
  if (changed) {
    state.sampleToken += 1;
    state.sampleBusy = false;
    state.files = { a: null, b: null, c: null };
    ["a", "b", "c"].forEach((role) => {
      const input = $("file" + role.toUpperCase());
      if (input) input.value = "";
      renderRole(role);
    });
    $("resultPanel").classList.add("hidden");
    $("setupPanel").classList.remove("hidden");
  }
  if (updateUrl) {
    const url = new URL(window.location.href);
    url.searchParams.set("scenario", scenario.id);
    window.history.replaceState(null, "", url);
  }
  renderMeta();
}

function syncSampleButtons() {
  ["samplePassBtn", "sampleRejectBtn"].forEach((id) => {
    const button = $(id);
    if (button) button.disabled = state.sampleBusy || state.busy || !state.backend.exists;
  });
}

function parseItems(text) {
  const items = [];
  const seen = new Set();
  for (const raw of String(text).split(/\r?\n/)) {
    const value = raw.trim();
    if (!value || seen.has(value)) continue;
    seen.add(value);
    items.push(value);
    if (items.length > MAX_ITEMS) {
      throw new Error(`单方去重后超过 ${fmt(MAX_ITEMS)} 条，请先抽样`);
    }
  }
  return items;
}

async function hashItems(items) {
  const hashes = new Array(items.length);
  const map = new Map();
  const started = performance.now();
  for (let index = 0; index < items.length; index += 1) {
    const value = items[index];
    const hash = window.sha256Hex(value).slice(0, 32);
    hashes[index] = hash;
    if (!map.has(hash)) map.set(hash, value);
    if (index % 4000 === 3999) await new Promise((resolve) => setTimeout(resolve, 0));
  }
  return { hashes, map, ms: performance.now() - started };
}

async function loadText(role, name, text) {
  const label = role.toUpperCase();
  $("count" + label).textContent = "读取中…";
  try {
    const items = parseItems(text);
    if (!items.length) throw new Error("文件里没有有效行");
    const hashed = await hashItems(items);
    state.files[role] = { name, items, ...hashed };
    renderRole(role);
    renderMeta();
  } catch (error) {
    state.files[role] = null;
    $("count" + label).textContent = "读取失败";
    $("preview" + label).textContent = error.message;
    renderMeta();
    throw error;
  }
}

async function loadFile(role, file) {
  if (!file) return;
  const label = role.toUpperCase();
  if (file.size > MAX_FILE_BYTES) {
    state.files[role] = null;
    $("count" + label).textContent = "读取失败";
    $("preview" + label).textContent = "文件超过 12MB，请先抽样";
    renderMeta();
    toast(`${label} 方：文件超过 12MB，请先抽样`);
    return;
  }
  try {
    await loadText(role, file.name, await file.text());
  } catch (error) {
    toast(`${label} 方：${error.message}`);
  }
}

async function loadSamplesAndRun(threshold) {
  if (state.sampleBusy || state.busy) return;
  if (!state.backend.exists) {
    toast("后端未配置：请设置 MYPSI_BIN");
    return;
  }
  const scenario = state.scenario;
  if (!scenario) {
    toast("场景尚未加载完成");
    return;
  }
  const token = ++state.sampleToken;
  state.sampleBusy = true;
  syncSampleButtons();
  try {
    const roles = ["A", "B", "C"];
    const responses = await Promise.all(roles.map((role) => (
      fetch(`/static/samples/${scenario.id}_${role}.txt`, { cache: "no-store" })
    )));
    const texts = await Promise.all(responses.map(async (response) => {
      if (!response.ok) throw new Error(`样例下载失败：HTTP ${response.status}`);
      return response.text();
    }));
    if (token !== state.sampleToken || state.scenario !== scenario) return;
    state.files = { a: null, b: null, c: null };
    await Promise.all(roles.map((role, index) => (
      loadText(role.toLowerCase(), `${scenario.id}_${role}.txt`, texts[index])
    )));
    if (token !== state.sampleToken || state.scenario !== scenario) return;
    $("threshold").value = String(threshold);
    renderMeta();
    toast(`样例已载入，正在运行（k=${threshold}）`);
    await run();
  } catch (error) {
    if (token === state.sampleToken) toast(`一键样例失败：${error.message}`);
  } finally {
    if (token === state.sampleToken) {
      state.sampleBusy = false;
      syncSampleButtons();
    }
  }
}

function renderRole(role) {
  const label = role.toUpperCase();
  const entry = state.files[role];
  if (!entry) {
    $("count" + label).textContent = "未选择";
    const format = state.scenario ? state.scenario.element_format : "元素";
    $("preview" + label).textContent = `每行一个${format}；CSV 请先抽成一列`;
    return;
  }
  $("count" + label).textContent = `${fmt(entry.items.length)} 条`;
  const preview = entry.items.slice(0, 4).join("\n");
  $("preview" + label).textContent = `${preview}${entry.items.length > 4 ? "\n…" : ""}\n\n本地哈希 ${entry.ms.toFixed(2)}s · ${entry.name}`;
}

function capacity() {
  const sizes = ["a", "b", "c"].map((role) => (state.files[role] ? state.files[role].items.length : 0));
  const largest = Math.max(0, ...sizes);
  if (!largest) return { largest, nLog: 0, n: 0 };
  const nLog = Math.max(8, Math.ceil(Math.log2(largest)));
  return { largest, nLog, n: 2 ** nLog };
}

function thresholdStatus() {
  const ready = ["a", "b", "c"].every((role) => Boolean(state.files[role]));
  const sizes = ["a", "b", "c"].map((role) => (state.files[role] ? state.files[role].items.length : 0));
  const max = ready ? Math.min(...sizes) : 0;
  const raw = $("threshold").value.trim();
  let message = "";
  let value = null;
  if (!ready) {
    message = "载入 A、B、C 三份文件后可设置门限";
  } else if (!raw) {
    message = "请输入正整数门限 k";
  } else if (!/^\d+$/.test(raw)) {
    message = "门限必须是正整数";
  } else {
    value = Number(raw);
    if (!Number.isSafeInteger(value) || value < 1) message = "门限至少为 1";
    else if (value > max) message = `门限不能超过三方最小集合 ${fmt(max)} 条`;
  }
  return { ready, max, value, message, valid: ready && !message };
}

function updateThresholdFeedback() {
  const status = thresholdStatus();
  const input = $("threshold");
  const error = $("thresholdError");
  input.max = status.max ? String(status.max) : "";
  input.classList.toggle("invalid", status.ready && Boolean(status.message));
  input.setAttribute("aria-invalid", String(status.ready && Boolean(status.message)));
  error.textContent = status.ready ? status.message : "";
  error.classList.toggle("hidden", !status.ready || !status.message);
  return status;
}

function renderMeta() {
  const ready = ["a", "b", "c"].every((role) => state.files[role]);
  const sizes = ["a", "b", "c"].map((role) => (state.files[role] ? state.files[role].items.length : 0));
  const cap = capacity();
  const threshold = $("threshold");
  const thresholdLabel = state.scenario ? state.scenario.threshold_label : "交集门限 k";
  $("thresholdLabel").textContent = thresholdLabel;
  if (ready && cap.n > 0) {
    $("realMeta").innerHTML =
      `去重后：A <b>${fmt(sizes[0])}</b> / B <b>${fmt(sizes[1])}</b> / C <b>${fmt(sizes[2])}</b> 条；` +
      `协议容量 <b>2^${cap.nLog} = ${fmt(cap.n)}</b>（按最大集合自动取）。` +
      (state.scenario ? `<br>${scenarioRoleText(state.scenario)}；${state.scenario.real_note}` : "");
    const maxThreshold = Math.min(...sizes);
    $("thresholdHint").textContent = `${thresholdLabel}：可填 1..${fmt(maxThreshold)}（三方最小集合）；超过会显示红色提示，无法开始。`;
  } else {
    $("realMeta").textContent = state.scenario
      ? `选择三份文件后，这里显示去重条数与协议容量。${scenarioExampleText(state.scenario)}`
      : "选择三份文件后，这里显示去重条数与协议容量。";
    $("thresholdHint").textContent = `${thresholdLabel}：交集 ≥ k 才输出；未达到输出 ⊥。`;
  }
  const status = updateThresholdFeedback();
  $("runBtn").disabled = !status.valid || !state.backend.exists || cap.nLog > 17;
}

function stopTimers() {
  if (state.timer) clearInterval(state.timer);
  if (state.poller) clearInterval(state.poller);
  state.timer = null;
  state.poller = null;
}

function stageText(progress) {
  const stage = Number(progress.stage || 0);
  if (progress.decision === "accept") return "已得到结果（达到门限）";
  if (progress.decision === "reject") return "已得到结果（未达到门限，输出 ⊥）";
  if (stage >= 4) return "交集阶段完成，整理输出…";
  if (stage === 3) return "份额收发完成，进入门限比较…";
  if (stage === 2) return "离线阶段完成（编码 + OPRF），进入份额收发…";
  if (progress.message) return progress.message;
  return "启动两个发送方（端口 1212/1213）…";
}

async function pollProgress(token, startedAt) {
  try {
    const response = await fetch("/api/progress", { cache: "no-store" });
    const data = await response.json();
    if (token !== state.token) return;
    $("progressStage").textContent = stageText(data);
    const percent = Number(data.percent || 5);
    $("progressBar").style.width = `${Math.max(5, percent)}%`;
    const parts = [`已运行 ${((performance.now() - startedAt) / 1000).toFixed(1)}s`];
    if (data.offline_s != null) parts.push(`离线 ${Number(data.offline_s).toFixed(2)}s`);
    if (data.share_s != null) parts.push(`份额 ${Number(data.share_s).toFixed(2)}s`);
    if (data.intersection_s != null) parts.push(`交集 ${Number(data.intersection_s).toFixed(2)}s`);
    if (data.intersection_size != null) parts.push(`交集规模 ${fmt(data.intersection_size)}`);
    $("progressStats").textContent = parts.join(" · ");
  } catch (_) {
    /* keep the last known progress */
  }
}

async function run() {
  if (state.busy) return;
  const ready = ["a", "b", "c"].every((role) => state.files[role]);
  if (!ready) {
    toast("先选择 A、B、C 三份文件");
    return;
  }
  if (!state.backend.exists) {
    toast("后端未配置：请设置 MYPSI_BIN");
    return;
  }
  const cap = capacity();
  if (cap.nLog > 17) {
    toast("超过 2^17，请先抽样到 131,072 条以内");
    return;
  }
  const thresholdCheck = thresholdStatus();
  if (!thresholdCheck.valid) {
    updateThresholdFeedback();
    toast(thresholdCheck.message);
    return;
  }
  const threshold = thresholdCheck.value;

  state.busy = true;
  state.result = null;
  state.token += 1;
  const token = state.token;
  $("runBtn").disabled = true;
  $("errorPanel").classList.add("hidden");
  $("resultPanel").classList.add("hidden");
  $("progressPanel").classList.remove("hidden");
  $("setupPanel").classList.add("hidden");
  $("progressTimer").textContent = "0.00s";
  $("progressBar").style.width = "5%";
  $("progressStage").textContent = "启动两个发送方（端口 1212/1213）…";
  $("progressStats").textContent = "等待后端日志…";

  const startedAt = performance.now();
  state.timer = setInterval(() => {
    if (token === state.token) {
      $("progressTimer").textContent = `${((performance.now() - startedAt) / 1000).toFixed(2)}s`;
    }
  }, 100);
  state.poller = setInterval(() => pollProgress(token, startedAt), 700);

  const payload = {
    mode: "file",
    data_mode: "files",
    scenario: state.scenario ? state.scenario.id : "ad",
    threshold,
    set_a: state.files.a.hashes.join("\n"),
    set_b: state.files.b.hashes.join("\n"),
    set_c: state.files.c.hashes.join("\n"),
  };

  try {
    const response = await fetch("/api/run", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error || `HTTP ${response.status}`);
    if (token !== state.token) return;
    stopTimers();
    state.busy = false;
    $("runBtn").disabled = false;
    $("progressBar").style.width = "100%";
    $("progressStage").textContent = "完成";
    state.result = data;
    renderResult(data);
  } catch (error) {
    if (token !== state.token) return;
    stopTimers();
    fail(error.message || String(error), error.stack || "");
  }
}

function metric(label, value) {
  return `<div class="metric"><span>${label}</span><strong>${value}</strong></div>`;
}

function mappedIntersection(result) {
  const map = state.files.a ? state.files.a.map : new Map();
  const fallbackB = state.files.b ? state.files.b.map : new Map();
  const fallbackC = state.files.c ? state.files.c.map : new Map();
  return (result.intersection_hashes || []).map((hash) => map.get(hash) || fallbackB.get(hash) || fallbackC.get(hash) || hash);
}

function renderResult(result) {
  const accept = result.decision === "accept";
  const scenario = scenarioById(result.scenario) || state.scenario || {};
  const thresholdLabel = scenario.threshold_label || "交集门限 k";
  const card = $("decisionCard");
  card.classList.toggle("accept", accept);
  card.classList.toggle("reject", !accept);
  $("decisionBadge").textContent = accept ? "达到门限" : "未达到门限";
  $("resultBig").textContent = accept ? fmt(result.intersection_size) : "⊥";
  $("resultSub").textContent = accept
    ? `交集 ${fmt(result.intersection_size)} 条 ≥ ${thresholdLabel} ${fmt(result.threshold)}`
    : `参考交集 ${fmt(result.reference_intersection)} 条 < ${thresholdLabel} ${fmt(result.threshold)}，协议输出 ⊥`;

  const m = result.metrics || {};
  $("metricGrid").innerHTML = [
    metric("场景 / 角色", `${scenario.name || result.scenario || "—"}<br>${scenarioRoleText(scenario)}`),
    metric("端到端墙钟", `${Number(result.wall_s || 0).toFixed(2)}s`),
    metric("协议总时间", `${Number(m.total_s || 0).toFixed(2)}s`),
    metric("离线 / 份额 / 交集", `${Number(m.offline_s || 0).toFixed(2)} / ${Number(m.share_s || 0).toFixed(2)} / ${Number(m.intersection_s || 0).toFixed(2)}s`),
    metric("通信量", `${Number(m.comm_mb || 0).toFixed(1)} MB`),
    metric("三方规模 A / B / C", `${fmt(result.size_a || 0)} / ${fmt(result.size_b || 0)} / ${fmt(result.size_c || 0)}`),
    metric("协议容量 / 门限", `2^${result.n_log} = ${fmt(result.n)} / k=${fmt(result.threshold)}`),
    metric("参考交集（仅讲解）", result.reference_intersection == null ? "—" : `${fmt(result.reference_intersection)} 条`),
  ].join("");

  const items = mappedIntersection(result);
  const preview = items.slice(0, 8).map((item, index) => `${String(index + 1).padStart(2, "0")}. ${item}`).join("\n");
  $("resultPreview").textContent = accept
    ? (preview + (items.length > 8 ? `\n… 另有 ${fmt(items.length - 8)} 条` : ""))
    : "未达到门限：协议没有输出交集元素（上方“交集条数”仅用于讲解）。";

  $("actionNote").textContent = accept ? (scenario.action_accept || "") : (scenario.action_reject || "");
  $("demoNote").textContent =
    `${scenario.name || result.scenario || "本次场景"}：${scenarioRoleText(scenario)}；` +
    `A ${fmt(result.size_a)} / B ${fmt(result.size_b)} / C ${fmt(result.size_c)} 条，` +
    `容量 2^${result.n_log}；交集由三方 Qin-3 协议真实计算，浏览器只上传 128-bit 哈希。` +
    ` 本次 ${thresholdLabel}=${fmt(result.threshold)}。` +
    (scenario.real_note ? ` ${scenario.real_note}` : "") +
    (accept
      ? (result.reference_matches_protocol ? " 协议输出与服务端参考值一致。" : "")
      : " 未达门限时协议不返回元素；上方“参考交集”是服务端对三份哈希本地比对的讲解值，不作为协议输出。");
  $("resultPanel").classList.remove("hidden");
  $("progressPanel").classList.add("hidden");
  $("setupPanel").classList.add("hidden");
  $("resultPanel").scrollIntoView({ behavior: "smooth", block: "start" });
}

function conclusionText(result) {
  const m = result.metrics || {};
  const accept = result.decision === "accept";
  const scenario = scenarioById(result.scenario) || state.scenario || {};
  const thresholdLabel = scenario.threshold_label || "交集门限 k";
  return [
    "【PSI 真实数据运行结论】",
    `场景：${scenario.name || result.scenario || "—"}；角色：${scenarioRoleText(scenario)}`,
    `门限含义：${thresholdLabel}`,
    `规模：A ${fmt(result.size_a)} / B ${fmt(result.size_b)} / C ${fmt(result.size_c)} 条；协议容量 2^${result.n_log} = ${fmt(result.n)}`,
    `门限：${thresholdLabel} = ${fmt(result.threshold)}`,
    accept
      ? `结果：达到门限，交集 ${fmt(result.intersection_size)} 条；${scenario.action_accept || ""}`
      : `结果：未达到门限（⊥），参考交集 ${fmt(result.reference_intersection)} 条（协议未输出）；${scenario.action_reject || ""}`,
    `耗时：墙钟 ${Number(result.wall_s || 0).toFixed(2)}s；协议总 ${Number(m.total_s || 0).toFixed(2)}s（离线 ${Number(m.offline_s || 0).toFixed(2)}s / 份额 ${Number(m.share_s || 0).toFixed(2)}s / 交集 ${Number(m.intersection_s || 0).toFixed(2)}s）；通信 ${Number(m.comm_mb || 0).toFixed(1)} MB`,
    `run_id：${result.run_id}`,
    "说明：明文在浏览器本地哈希，服务端只收到 128-bit 哈希；未达门限时协议不输出具体交集。",
  ].join("\n");
}

async function copyConclusion() {
  if (!state.result) return;
  const text = conclusionText(state.result);
  try {
    await navigator.clipboard.writeText(text);
    toast("结论已复制");
  } catch (_) {
    const area = document.createElement("textarea");
    area.value = text;
    document.body.appendChild(area);
    area.select();
    document.execCommand("copy");
    area.remove();
    toast("结论已复制");
  }
}

function exportResult() {
  if (!state.result) return;
  const result = state.result;
  const payload = {
    app: "PSI Lab Demo · Real Data",
    exported_at: new Date().toISOString(),
    conclusion: conclusionText(result),
    intersection: mappedIntersection(result),
    result,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], { type: "application/json;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `psi-real-${result.run_id}.json`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1500);
  toast("已导出 JSON");
}

async function cancel() {
  state.token += 1;
  stopTimers();
  state.busy = false;
  $("runBtn").disabled = false;
  $("setupPanel").classList.remove("hidden");
  $("progressPanel").classList.add("hidden");
  try {
    const response = await fetch("/api/cancel", { method: "POST", headers: { "Content-Type": "application/json" }, body: "{}" });
    const data = await response.json();
    toast(data.cancelled ? "已终止后端进程" : "任务已结束");
  } catch (error) {
    toast(`取消失败：${error.message}`);
  }
}

async function boot() {
  ["a", "b", "c"].forEach((role) => {
    $("file" + role.toUpperCase()).addEventListener("change", (event) => {
      loadFile(role, event.target.files && event.target.files[0]);
    });
  });
  $("threshold").addEventListener("input", renderMeta);
  $("samplePassBtn").addEventListener("click", () => loadSamplesAndRun(200));
  $("sampleRejectBtn").addEventListener("click", () => loadSamplesAndRun(400));
  $("runBtn").addEventListener("click", run);
  $("cancelBtn").addEventListener("click", cancel);
  $("copyBtn").addEventListener("click", copyConclusion);
  $("exportBtn").addEventListener("click", exportResult);
  $("rerunBtn").addEventListener("click", () => {
    $("resultPanel").classList.add("hidden");
    $("setupPanel").classList.remove("hidden");
    $("setupPanel").scrollIntoView({ behavior: "smooth", block: "start" });
  });
  $("resetBtn").addEventListener("click", () => {
    state.files = { a: null, b: null, c: null };
    ["a", "b", "c"].forEach((role) => {
      $("file" + role.toUpperCase()).value = "";
      renderRole(role);
    });
    $("resultPanel").classList.add("hidden");
    $("setupPanel").classList.remove("hidden");
    renderMeta();
    $("setupPanel").scrollIntoView({ behavior: "smooth", block: "start" });
  });
  $("errorBackBtn").addEventListener("click", () => {
    $("errorPanel").classList.add("hidden");
    $("setupPanel").classList.remove("hidden");
    $("setupPanel").scrollIntoView({ behavior: "smooth", block: "start" });
  });
  $("errorToggleBtn").addEventListener("click", () => {
    $("errorDetail").classList.toggle("hidden");
    $("errorToggleBtn").textContent = $("errorDetail").classList.contains("hidden") ? "显示原始错误" : "收起原始错误";
  });

  try {
    const response = await fetch("/api/config", { cache: "no-store" });
    const config = await response.json();
    state.backend = config.backend || state.backend;
    state.scenarios = (config.scenarios || []).filter((scenario) => ["ad", "ride", "gene"].includes(scenario.id));
    renderScenarioTabs();
    const requested = new URLSearchParams(window.location.search).get("scenario") || "ad";
    applyScenario(requested, false);
    if (!state.backend.exists) {
      $("modePill").textContent = "未配置后端";
      $("realMeta").innerHTML = "未检测到 <b>MYPSI_BIN</b>，页面可以选文件但无法运行。请在服务器设置二进制路径后重启服务。";
    } else if (!state.backend.patched) {
      $("modePill").textContent = "后端未打补丁";
    }
    renderMeta();
  } catch (error) {
    toast(`读取配置失败：${error.message}`);
  }
  syncSampleButtons();
}

boot();
