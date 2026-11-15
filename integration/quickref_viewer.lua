-- Quickref Viewer has its own special workspace, leaving the original console intact.
local SPACE = "special:quickref-viewer"
local starting = false
local function fit(mon)
  if not mon or not mon.scale or mon.scale <= 0 then return end
  local w, h = mon.width, mon.height
  if mon.transform % 2 == 1 then w, h = h, w end
  local reserved = mon.reserved
  w = w / mon.scale - reserved.left - reserved.right
  h = h / mon.scale - reserved.top - reserved.bottom
  local height = math.floor(h * 0.65)
  local width = math.min(w, math.max(900, math.floor(w * 0.86)))
  hl.workspace_rule({ workspace = SPACE, gaps_in = 0,
    gaps_out = { top = 0, right = math.floor((w-width)/2), bottom = math.floor(h-height), left = math.floor((w-width)/2) },
    no_border = true,
  })
  hl.exec_scheduled_prop_refresh_immediately()
end
fit(hl.get_active_monitor())
hl.on("workspace.special_active", function(ws, mon)
  if ws and ws.name == SPACE then fit(mon) end
end)
hl.on("monitor.layout_changed", function()
  local ws = hl.get_workspace(SPACE)
  fit(ws and ws.visible and ws.monitor or hl.get_active_monitor())
end)
-- Keep the original console shortcut; use Super+Alt+grave for Quickref Viewer.
local function toggle()
  fit(hl.get_active_monitor())
  if #hl.get_workspace_windows(SPACE) == 0 and not starting then
    local binary = os.getenv("HOME") .. "/.config/omarchy/plugins/io.github.qrooto.quickref-viewer/build/quickref-viewer"
    local file = io.open(binary, "rb")
    if not file then error("Build Quickref Viewer first; see README.md") end
    file:close()
    starting = true
    hl.exec_cmd(o.shell_quote(binary) .. " open", { workspace = SPACE .. " silent" })
    hl.timer(function() starting = false end, { timeout = 2500, type = "oneshot" })
  end
  hl.dispatch(hl.dsp.workspace.toggle_special("quickref-viewer"))
end
o.rebind("SUPER + ALT + grave", "Quickref Viewer", toggle)
local function open_external(path)
  local mon = hl.get_active_monitor()
  local regular = mon and mon.active_workspace
  -- The monitor API reports the underlying regular workspace separately.
  local workspace = regular and regular.name
  if not workspace then error("Cannot determine the regular workspace") end
  local panel = hl.get_workspace(SPACE)
  if panel and panel.visible then
    hl.dispatch(hl.dsp.workspace.toggle_special("quickref-viewer"))
  end
  hl.exec_cmd("gio open " .. o.shell_quote(path), { workspace = workspace })
end
return { toggle = toggle, open_external = open_external }
