const $ = (id) => document.getElementById(id);
let frameRevision = -1;
function showError(message) {
  $("error").hidden = !message;
  $("error").textContent = message || "";
}
async function send(kind, value) {
  try {
    const r = await fetch("/input", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ kind, value }),
    });
    if (!r.ok) throw Error("Input was rejected");
    return true;
  } catch (e) {
    showError(e.message);
    return false;
  }
}
function physicalButton(element, kind) {
  element.onpointerdown = (e) => {
    element.setPointerCapture(e.pointerId);
    send(kind, true);
  };
  element.onpointerup = element.onpointercancel = () => send(kind, false);
  element.onkeydown = (e) => {
    if ([" ", "Enter"].includes(e.key)) {
      e.preventDefault();
      if (!e.repeat) send(kind, true);
    }
  };
  element.onkeyup = (e) => {
    if ([" ", "Enter"].includes(e.key)) {
      e.preventDefault();
      send(kind, false);
    }
  };
}
physicalButton($("display"), "touch");
for (const kind of ["boot", "pwr"])
  $(kind).onclick = () =>
    send(kind, $(kind).getAttribute("aria-pressed") !== "true");
$("key").onclick = () => send("key");
$("supply-on").onclick = () => send("supply", true);
$("supply-off").onclick = () => send("supply", false);
window.addEventListener("blur", () => send("touch", false));
$("orientation").onchange = (e) => send("rotate", +e.target.value);
let canSend = false,
  sending = false,
  commandHistory = [],
  historyIndex = 0,
  draft = "";
const commands = ["STATUS", "SHOW", "DIM ", "TIME ", "ACCEL", "POWER", "DIAG"];
function commandReady() {
  $("send-command").disabled =
    !canSend || sending || !$("command").value.trim();
}
$("command").oninput = () => {
  historyIndex = commandHistory.length;
  commandReady();
};
$("command").onkeydown = (e) => {
  if (e.isComposing) return;
  if (e.key === "ArrowUp" || e.key === "ArrowDown") {
    e.preventDefault();
    if (historyIndex === commandHistory.length) draft = e.target.value;
    historyIndex = Math.max(
      0,
      Math.min(
        commandHistory.length,
        historyIndex + (e.key === "ArrowUp" ? -1 : 1),
      ),
    );
    e.target.value =
      historyIndex === commandHistory.length
        ? draft
        : commandHistory[historyIndex];
    commandReady();
  } else if (e.key === "Tab" && e.target.value.trim()) {
    const matches = commands.filter((c) =>
      c.startsWith(e.target.value.toUpperCase()),
    );
    if (matches.length === 1) {
      e.preventDefault();
      e.target.value = matches[0];
      commandReady();
    }
  }
};
$("command-form").onsubmit = async (e) => {
  e.preventDefault();
  const input = $("command");
  const value = input.value.trim();
  if (!canSend || sending || !value) return;
  sending = true;
  commandReady();
  const ok = await send("serial", value);
  sending = false;
  if (ok) {
    if (commandHistory.at(-1) !== value) commandHistory.push(value);
    commandHistory = commandHistory.slice(-32);
    historyIndex = commandHistory.length;
    draft = "";
    if (input.value.trim() === value) input.value = "";
  }
  $("command-feedback").textContent = ok ? "Sent · " + value : "Send failed";
  commandReady();
  input.focus();
};
document
  .querySelectorAll("[data-address]")
  .forEach(
    (e) =>
      (e.onchange = () =>
        send("fault", { address: +e.dataset.address, enabled: e.checked })),
  );
async function poll() {
  try {
    const r = await fetch("/state");
    if (!r.ok) throw Error("State unavailable");
    const s = await r.json();
    $("status").textContent = s.error
      ? "Emulation stopped"
      : s.powered === false
        ? "Power off"
        : s.bootloader
          ? "ROM download"
          : s.sleeping
            ? "Standby"
            : s.on
              ? "Display active"
              : "Starting";
    const available =
      s.powered !== false && !s.bootloader && !s.sleeping && !s.error;
    if (available !== canSend) {
      canSend = available;
      $("command-feedback").textContent = canSend
        ? "Enter to send"
        : "Wake board to send";
    }
    commandReady();
    $("connection").className =
      "status " + (s.error ? "error" : s.sleeping ? "" : "awake");
    showError(s.error);
    for (const kind of ["boot", "pwr"]) {
      $(kind).setAttribute("aria-pressed", String(!!s[kind + "_down"]));
      $(kind + "-state").textContent = s[kind + "_down"] ? "down" : "up";
    }
    $("supply-on").disabled = s.supply_on !== false;
    $("supply-off").disabled = s.supply_on === false;
    $("key").disabled = !!s.error || s.powered === false || !!s.bootloader;
    $("hold-state").textContent =
      "Display hold " +
      (s.held === true ? "on" : s.held === false ? "off" : "unknown");
    $("hold-state").className = "badge " + (s.held ? "active" : "");
    $("wake-hint").textContent =
      s.supply_on === false
        ? "5V disconnected"
        : s.powered === false
          ? "Press PWR to start"
          : s.bootloader
            ? "ROM download not modeled"
            : s.sleeping
              ? "Touch to wake"
              : s.held
                ? "Display stays awake"
                : "Touch to restart the interval";
    $("brightness").textContent =
      s.brightness == null ? "—" : Math.round((s.brightness / 255) * 100) + "%";
    $("transactions").textContent = (s.transactions || 0).toLocaleString();
    $("pixels").textContent = (s.pixels || 0).toLocaleString();
    const log = $("console");
    if (log.textContent !== s.console) {
      log.textContent = s.console;
      log.scrollTop = log.scrollHeight;
    }
    $("display").style.opacity = s.on ? s.brightness / 255 : 0;
    if (s.frame !== frameRevision) {
      frameRevision = s.frame;
      $("display").src = "/frame?t=" + frameRevision;
    }
  } catch (e) {
    canSend = false;
    commandReady();
    $("command-feedback").textContent = "Simulator disconnected";
    $("status").textContent = "Disconnected";
    $("connection").className = "status error";
    showError("Cannot reach the simulator. Start it with make emulate.");
  } finally {
    setTimeout(poll, 16);
  }
}
function node(id, label, tone = "") {
  const e = $(id);
  e.querySelector(".node-state").textContent = label;
  e.classList.remove("live", "wait");
  if (tone) e.classList.add(tone);
}
function diagram(data) {
  const e = data["Execution (modeled)"];
  const off = e["HP CPU"] === "powered off";
  const boot = e["HP CPU"].startsWith("ROM download");
  const sleep = e["HP CPU"] === "deep sleep";
  const waiting = e["HP CPU"] === "waiting (WFI)";
  $("power-mode").textContent = off
    ? "Power off"
    : boot
      ? "ROM download"
      : sleep
        ? "Sleep policy"
        : "Active policy";
  $("power-mode").className =
    "badge " + (!off && !boot && !sleep ? "active" : "");
  node(
    "hp",
    off
      ? "Powered off"
      : boot
        ? "ROM not modeled"
        : sleep
          ? "Deep sleep"
          : waiting
            ? "Waiting · WFI"
            : "Executing",
    off || boot || sleep ? "" : waiting ? "wait" : "live",
  );
  node("lp", "Not executed");
  node(
    "hp-ram",
    off ? "Reset on power-on" : "Mapped · modeled",
    off ? "" : "live",
  );
  node(
    "ram",
    off ? "Reset on power-on" : "Retained · modeled",
    off ? "" : "live",
  );
  const policy =
    data[(sleep ? "Sleep" : "Active") + " power policy (programmed)"];
  for (const [id, key] of [
    ["top", "HP TOP"],
    ["aon", "HP AON"],
  ]) {
    const v = policy[key];
    node(
      id,
      off
        ? "Power off"
        : v === "power-down requested"
          ? "Off req."
          : v === "no power-down request"
            ? "On policy"
            : "Unknown",
      !off && v === "no power-down request" ? "live" : "",
    );
  }
  const r =
    data["Power and watchdog registers (programmed)"]["Radio force control"];
  node(
    "radio",
    off
      ? "Power off"
      : r === "not written"
        ? "Not configured"
        : parseInt(r) & 32
          ? "Forced off · request"
          : "No force-off request",
  );
  const wake = parseInt(data["GPIO and timer"]["Wake source enable"]);
  const armed = !off && !boot && Number.isFinite(wake) && !!(wake & 4);
  node(
    "touch-node",
    armed ? "Touch wake armed" : "Wake not armed",
    armed ? "live" : "",
  );
  $("usb-module").textContent = off
    ? "Powered off"
    : boot
      ? "ROM not modeled"
      : sleep
        ? "FIFO · standby"
        : "FIFO only · available";
  $("pc").textContent = e.PC;
  $("elapsed").textContent = e["Elapsed seconds"].toFixed(1) + " s";
}
async function inspect() {
  try {
    const r = await fetch("/inspect");
    if (!r.ok) throw Error("Inspection unavailable");
    const data = await r.json();
    diagram(data);
    const root = $("inspection");
    root.replaceChildren();
    for (const [group, values] of Object.entries(data)) {
      const title = document.createElement("h3");
      title.textContent = group;
      const table = document.createElement("table");
      for (const [name, value] of Object.entries(values)) {
        const row = table.insertRow();
        row.insertCell().textContent = name;
        row.insertCell().textContent = String(value);
      }
      root.append(title, table);
    }
  } catch (e) {
    $("power-mode").textContent = "Unavailable";
  } finally {
    setTimeout(inspect, 250);
  }
}
poll();
inspect();
