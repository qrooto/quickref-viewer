-- Exercise the compositor module without touching the running desktop.
local rules, binding, launch, dispatches, timer
local windows = {}
local shown = false
local mon = {width=1920,height=1080,scale=1,transform=0,
  reserved={left=0,right=0,top=0,bottom=0},active_workspace={name="3"}}
hl = {
  get_active_monitor=function() return mon end,
  workspace_rule=function(value) rules=value end,
  exec_scheduled_prop_refresh_immediately=function() end,
  on=function() end,
  get_workspace=function() return {visible=shown,monitor=mon} end,
  get_workspace_windows=function() return windows end,
  exec_cmd=function(command,options) launch={command,options} end,
  timer=function(callback) timer=callback end,
  dispatch=function(value) dispatches=(dispatches or 0)+1; shown=not shown end,
  dsp={workspace={toggle_special=function(name) assert(name=="quickref-viewer"); return name end}}
}
o = {
  rebind=function(keys,_,callback) binding={keys,callback} end,
  shell_quote=function(value) return "'"..value:gsub("'", "'\\''").."'" end
}
local realOpen=io.open
io.open=function(path,mode)
  local suffix="/io.github.qrooto.quickref-viewer/build/quickref-viewer"
  assert(path:sub(-#suffix)==suffix)
  return {close=function() end}
end
local module=dofile(arg[1])
assert(binding[1]=="SUPER + ALT + grave")
assert(rules.workspace=="special:quickref-viewer")
assert(rules.gaps_out.top==0 and rules.gaps_out.bottom==378)
module.toggle()
assert(dispatches==1 and shown)
assert(launch[2].workspace=="special:quickref-viewer silent")
local firstLaunch=launch
module.toggle()
assert(dispatches==2 and not shown and launch==firstLaunch)
timer(); windows={1}; module.toggle()
assert(shown and launch==firstLaunch)
module.open_external("/tmp/reader's note.md")
assert(not shown and launch[2].workspace=="3")
assert(launch[1]:match("^gio open "))
mon.scale=2
module.toggle()
assert(rules.gaps_out.top==0 and rules.gaps_out.bottom==189)
io.open=realOpen
print("Drop-down tests passed: placement, isolated hotkey/workspace, launch guard, hide/show, editor destination, scaling")
