param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Fm1OpenRoot
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Fm1OpenRoot).Path
$app = Join-Path $root 'app'
$screenPath = Join-Path $app 'screen.c'
$midiRouterPath = Join-Path $app 'midi_router.c'
$apiPath = Join-Path $root 'sdk\include\fm1.h'
$readmePath = Join-Path $root 'README.md'
if (!(Test-Path -LiteralPath $screenPath)) { throw "FM1 Open app/screen.c not found under $root" }
if (!(Test-Path -LiteralPath $midiRouterPath)) { throw "FM1 Open app/midi_router.c not found under $root" }
if (!(Test-Path -LiteralPath $apiPath) -or !(Test-Path -LiteralPath $readmePath)) { throw "FM1 Open SDK files not found under $root" }
$readme = [IO.File]::ReadAllText($readmePath)
$api = [IO.File]::ReadAllText($apiPath)
if ($readme -notmatch '(?m)^# FM1 Open V14\s*$' -or $api -notmatch '(?m)^#define FM1_API_VERSION 1U\s*$' -or
    $api -notmatch '(?m)^void fm1_controls_init\(void\);' -or $api -notmatch '(?m)^void fm1_screen_init\(void\);' -or
    $api -notmatch '(?m)^int fm1_screen_submit\(') {
    throw 'This installer requires the FM1 Open V14 SDK API used by this game.'
}

function Write-Utf8NoBom([string]$path, [string]$text) {
    [IO.File]::WriteAllText($path, $text, [Text.UTF8Encoding]::new($false))
}

foreach ($name in @('staff_game.c', 'staff_game.h', 'staff_game_fm1.c', 'staff_game_fm1.h', 'daughter_qing_events.h')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "src\$name") -Destination (Join-Path $app $name) -Force
}

$screen = [IO.File]::ReadAllText($screenPath)
$newline = if ($screen.Contains("`r`n")) { "`r`n" } else { "`n" }
$marker = '#include "fm1_engine_control.h"'
$guard = '#define FM1_STAFF_GAME_ONLY 1'
$branch = @'
#include "staff_game_fm1.h"
#define FM1_STAFF_GAME_ONLY 1
#ifdef FM1_STAFF_GAME_ONLY
#include "fm1.h"

/* Preserve SDK-validated BLE/project hooks after removing their stock pages. */
static int (* volatile keep_ble_role)(enum fm1_ble_role) = fm1_ble_set_role;
static int (* volatile keep_ble_scan)(void) = fm1_ble_scan;
static int (* volatile keep_ble_connect)(unsigned) = fm1_ble_connect;
static int (* volatile keep_ble_disconnect)(void) = fm1_ble_disconnect;
static void (* volatile keep_ble_status)(struct fm1_ble_status *) = fm1_ble_get_status;
static int (* volatile keep_ble_device)(unsigned, struct fm1_ble_device *) = fm1_ble_get_device;
static void (* volatile keep_bluetooth_status)(struct fm1_bluetooth_status *) = fm1_bluetooth_get_status;
static int (* volatile keep_tuning_capture)(struct groove_tuning *) = groove_tuning_capture;
static int (* volatile keep_tuning_apply)(const struct groove_tuning *) = groove_tuning_apply;
static int (* volatile keep_project_edit)(const struct groove_project *, unsigned) = groove_project_apply_edit;

void groove_screen_init(void)
{
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
    (void)keep_ble_role; (void)keep_ble_scan; (void)keep_ble_connect;
    (void)keep_ble_disconnect; (void)keep_ble_status; (void)keep_ble_device;
    (void)keep_bluetooth_status;
    (void)keep_tuning_capture; (void)keep_tuning_apply; (void)keep_project_edit;
}
void groove_screen_tap(uint32_t now) { (void)now; }
void groove_screen_control(void)
{
    struct fm1_controls controls;
    fm1_controls_read(&controls);
    (void)groove_master_update(controls.knob);
    (void)staff_game_fm1_control(&controls);
}
void groove_screen_mmc(unsigned command) { (void)command; }

#else
'@
$branch = [regex]::Replace($branch, "`r?`n", $newline)
$screenChanged = $false
if (!$screen.Contains($guard)) {
    if (!$screen.Contains($marker)) { throw 'Could not find the expected FM1 Open screen include anchor.' }
    $screen = $screen.Replace($marker, $marker + $newline + $branch)
    $screen += $newline + '#endif /* FM1_STAFF_GAME_ONLY */' + $newline
    Write-Utf8NoBom $screenPath $screen
    $screenChanged = $true
} else {
    # Refresh the injected game block on reruns so fixes to the adapter are not
    # silently skipped just because an earlier install already added the guard.
    $guardIndex = $screen.IndexOf($guard, [StringComparison]::Ordinal)
    $branchStart = $screen.LastIndexOf('#include "staff_game_fm1.h"', $guardIndex, [StringComparison]::Ordinal)
    $branchEnd = $screen.IndexOf('#else', $guardIndex, [StringComparison]::Ordinal)
    $branchElse = $branch.LastIndexOf('#else', [StringComparison]::Ordinal)
    if ($branchStart -lt 0 -or $branchEnd -lt 0 -or $branchElse -lt 0) {
        throw 'Could not find the existing FM1 staff-game screen block to refresh.'
    }
    $updatedBranch = $branch.Substring(0, $branchElse) + $screen.Substring($branchEnd)
    if ($screen.Substring($branchStart) -cne $updatedBranch) {
        $screen = $screen.Substring(0, $branchStart) + $updatedBranch
        Write-Utf8NoBom $screenPath $screen
        $screenChanged = $true
    }
}

$midiRouter = [IO.File]::ReadAllText($midiRouterPath)
$routerNewline = if ($midiRouter.Contains("`r`n")) { "`r`n" } else { "`n" }
$routerInclude = '#include "staff_game_fm1.h"'
$routerAnchor = '#include "fm1_engine_control.h"'
if (!$midiRouter.Contains($routerInclude)) {
    if (!$midiRouter.Contains($routerAnchor)) { throw 'Could not find the expected FM1 Open MIDI router include anchor.' }
    $midiRouter = $midiRouter.Replace($routerAnchor, $routerAnchor + $routerNewline + $routerInclude)
}
$oldPost = '(void)fm1_midi_post(m, n);'
$gamePost = 'if (!staff_game_fm1_midi_received(m, n)) (void)fm1_midi_post(m, n);'
if (!$midiRouter.Contains($gamePost)) {
    if (!$midiRouter.Contains($oldPost)) { throw 'Could not find the expected MIDI input dispatch.' }
    $midiRouter = $midiRouter.Replace($oldPost, $gamePost)
    Write-Utf8NoBom $midiRouterPath $midiRouter
}
if ($screenChanged) {
    Write-Output 'Installed the staff game screen and MIDI input.'
} else {
    Write-Output 'Updated the staff game MIDI input hook.'
}
