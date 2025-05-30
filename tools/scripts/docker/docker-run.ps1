param (
    [string]$builder,
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$cmd
)

function Find-DockerPath {
    # Try to find docker-compose.exe in PATH first
    $dockerCompose = & where.exe docker-compose.exe 2>$null | Select-Object -First 1
    if ($dockerCompose) { return $dockerCompose }

    # Then try to find docker.exe in PATH
    $docker = & where.exe docker.exe 2>$null | Select-Object -First 1
    if ($docker) { return $docker }

    # Try to read Docker install location from registry (for Docker Desktop)
    $regPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Docker Desktop"
    if (Test-Path $regPath) {
        $installLocation = (Get-ItemProperty -Path $regPath).InstallLocation
        if ($installLocation) {
            $dockerPathCandidate = Join-Path $installLocation "resources\bin\docker-compose.exe"
            if (Test-Path $dockerPathCandidate) { return $dockerPathCandidate }

            $dockerPathCandidate = Join-Path $installLocation "resources\bin\docker.exe"
            if (Test-Path $dockerPathCandidate) { return $dockerPathCandidate }
        }
    }

    Write-Error "Docker executable not found in PATH or registry."
    exit 1
}

$dockerPath = Find-DockerPath
Write-Host "Docker found at: $dockerPath"

$dockerFileName = [System.IO.Path]::GetFileName($dockerPath).ToLower()

if ($dockerFileName -eq "docker.exe") {
    $fullCmd = "& '$dockerPath' compose run --rm $builder $cmd"
} elseif ($dockerFileName -eq "docker-compose.exe") {
    $fullCmd = "& '$dockerPath' run --rm $builder $cmd"
} else {
    Write-Error "Unknown Docker executable: $dockerFileName"
    exit 1
}

Write-Host "Running: $fullCmd"
Invoke-Expression $fullCmd
