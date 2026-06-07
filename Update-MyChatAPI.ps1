#
# Update-MyChatAPI.ps1
#
# Refreshes the vendored MyChatAPI.h from its upstream GitHub source so MQMyDPS
# stays in sync with the MQMyChat plugin interface. MQMyDPS only needs this header
# to talk to a loaded MQMyChat via GetPluginInterface("MQMyChat") -- there is no link
# dependency, so the header is kept as a local copy and refreshed on demand (the same
# pattern as the vendored Nav_PluginAPI.h from brainiac/MQ2Nav).
#
# The script downloads the upstream header verbatim and prepends a local banner. The
# "already up to date" check compares only the upstream body (from `#pragma once` on),
# so re-running after adding the banner is a no-op until upstream actually changes.
#
# Upstream: https://github.com/grimmier378/MQMyChat (branch main, MyChatAPI.h)
# Usage:    pwsh -File Update-MyChatAPI.ps1
#

$ErrorActionPreference = 'Stop'

$Url  = 'https://raw.githubusercontent.com/grimmier378/MQMyChat/main/MyChatAPI.h'
$Dest = Join-Path $PSScriptRoot 'MyChatAPI.h'

$Banner = @'
//
// MyChatAPI.h
//
// Vendored from grimmier378/MQMyChat (MyChatAPI.h). MQMyDPS only needs this header
// to talk to a loaded MQMyChat via GetPluginInterface("MQMyChat"); there is no link
// dependency. Do NOT edit by hand -- refresh with Update-MyChatAPI.ps1.
//
// Upstream: https://github.com/grimmier378/MQMyChat (branch main)
//

'@

function Get-UpstreamBody([string]$text) {
    $i = $text.IndexOf('#pragma once')
    $body = if ($i -ge 0) { $text.Substring($i) } else { $text }
    return ($body -replace "`r`n", "`n")
}

Write-Host "Fetching $Url"
$remoteBody = Get-UpstreamBody (Invoke-WebRequest -Uri $Url -UseBasicParsing).Content
$desired    = $Banner + $remoteBody

if (Test-Path $Dest) {
    $current = (Get-Content -Raw -Path $Dest) -replace "`r`n", "`n"
    if ($current -eq ($desired -replace "`r`n", "`n")) {
        Write-Host "MyChatAPI.h is already up to date with upstream."
        return
    }
}

Set-Content -Path $Dest -Value $desired -NoNewline -Encoding utf8
Write-Host "Updated MyChatAPI.h from upstream."
