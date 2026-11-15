param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Clean', 'Build', 'Download')]
    [string]$Action,
    [string]$KeilPath
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$output = Join-Path $repo 'build\keil-heating'
$factory = Join-Path $output 'factory.hex'
$builder = Join-Path $PSScriptRoot 'build_upgrade.py'

function Find-Python {
    $installed = Get-Command python.exe -CommandType Application -All -ErrorAction SilentlyContinue |
        Where-Object Source -NotLike '*\Microsoft\WindowsApps\*' | Select-Object -First 1
    if ($installed) { return $installed.Source }

    $launcher = Get-Command py.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($launcher) { return $launcher.Source }

    $bundled = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $bundled -PathType Leaf) { return $bundled }
    throw 'Python 3 was not found. Install Python 3 and enable the py launcher or add Python to PATH.'
}

function Find-Programmer {
    $installed = Get-Command STM32_Programmer_CLI.exe -CommandType Application -ErrorAction SilentlyContinue
    if ($installed) { return $installed.Source }

    $relativePath = 'STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
    $candidates = @(
        (Join-Path $env:ProgramFiles $relativePath),
        (Join-Path ${env:ProgramFiles(x86)} $relativePath),
        (Join-Path $env:USERPROFILE '.eide\tools\st_cube_programer\bin\STM32_Programmer_CLI.exe')
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw 'STM32CubeProgrammer was not found. Install it or add STM32_Programmer_CLI.exe to PATH.'
}

function Invoke-DownloadStep {
    param([string]$Tool, [string[]]$ToolArgs)
    $ErrorActionPreference = 'Continue'
    & $Tool @ToolArgs 2>&1 | ForEach-Object { $_.ToString() } | Tee-Object -FilePath $downloadLog -Append
}

try {
    if ($Action -eq 'Clean') {
        # Remove the previous image before compiling so a failed build cannot be downloaded.
        if (Test-Path -LiteralPath $factory -PathType Leaf) {
            Remove-Item -LiteralPath $factory
        }
        exit 0
    }

    $python = Find-Python
    $pythonArgs = @('-B', $builder)
    if ((Split-Path -Leaf $python) -eq 'py.exe') { $pythonArgs = @('-3') + $pythonArgs }

    if ($Action -eq 'Build') {
        $app = Join-Path $output 'keil\heating.axf'
        $buildArgs = @('--version', 'V1.3', '--heating-app', '--factory-hex', '--keil-app', $app, '--out-dir', $output)
        if ($KeilPath) {
            $keilRoot = Split-Path -Parent (Split-Path -Parent $KeilPath)
            $armcc = Join-Path $keilRoot 'ARM\ARMCC'
            if (Test-Path -LiteralPath (Join-Path $armcc 'bin\fromelf.exe')) {
                $buildArgs += @('--toolchain', $armcc)
            }
        }
        & $python @pythonArgs @buildArgs
        exit $LASTEXITCODE
    }

    $downloadLog = Join-Path $output 'download.log'
    if (-not (Test-Path -LiteralPath $factory -PathType Leaf)) {
        throw 'Factory image is missing. Build the V1.3 Heating target before downloading.'
    }
    Set-Content -LiteralPath $downloadLog -Value 'V1.3 Heating SWD download' -Encoding Unicode
    $programmer = Find-Programmer
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ('microwave-swd-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temporary | Out-Null
    $image = Join-Path $temporary 'factory.hex'
    try {
        # Validate the same copy given to the programmer. Older CubeProgrammer builds need an ASCII image path.
        Copy-Item -LiteralPath $factory -Destination $image
        Invoke-DownloadStep $python ($pythonArgs + @('--verify-factory', $image))
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Invoke-DownloadStep $programmer @('-c', 'port=SWD', '-d', $image, '-v', '-rst')
        $result = $LASTEXITCODE
        Add-Content -LiteralPath $downloadLog -Value "Programmer exit code: $result"
        exit $result
    }
    finally {
        if (Test-Path -LiteralPath $image -PathType Leaf) { Remove-Item -LiteralPath $image }
        Remove-Item -LiteralPath $temporary
    }
}
catch {
    if ($downloadLog -and (Test-Path -LiteralPath $downloadLog -PathType Leaf)) {
        Add-Content -LiteralPath $downloadLog -Value $_.Exception.Message
    }
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
