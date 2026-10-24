# Regression: PowerShell splatting an array positionally must not collapse
# the Databricks CLI command to only its first word.
$ErrorActionPreference = 'Stop'
$path = Join-Path $PSScriptRoot '../scripts/Completar-4-Secretos-Alumno-3C.ps1'
$tokens = $null; $parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path $path).Path, [ref]$tokens, [ref]$parseErrors
)
if ($parseErrors.Count -gt 0) { throw 'PowerShell syntax error in target script.' }
$fn = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Invoke-DbJson'
}, $true)
if ($null -eq $fn) { throw 'Missing Invoke-DbJson function.' }
# Load only the function from our own checked-out repository.
Invoke-Expression $fn.Extent.Text

function Assert-Exit([string]$Contexto) {
    if ($global:LASTEXITCODE -ne 0) { throw "Databricks mock failure in $Contexto" }
}
function databricks {
    $script:CapturedArguments = @($args)
    $global:LASTEXITCODE = 0
    return $script:MockResponse
}

$expected = @('current-user', 'me', '--profile', 'alumno-3c', '--output', 'json')
$script:MockResponse = '{"userName":"polovaleriow@gmail.com"}'
$result = Invoke-DbJson -CliArgs $expected -Contexto 'usuario'
if ($result.userName -cne 'polovaleriow@gmail.com') { throw 'JSON was not parsed.' }
if ($script:CapturedArguments.Count -ne $expected.Count) {
    throw "CLI arguments collapsed: expected $($expected.Count), got $($script:CapturedArguments.Count)."
}
for ($i = 0; $i -lt $expected.Count; $i++) {
    if ($script:CapturedArguments[$i] -cne $expected[$i]) {
        throw "Wrong CLI argument at index $i."
    }
}
$script:MockResponse = 'Databricks: help instead of JSON'
$blocked = $false
try {
    $null = Invoke-DbJson -CliArgs $expected -Contexto 'usuario'
} catch {
    if ($_.Exception.Message -notmatch 'no es JSON valido') { throw }
    if ($_.Exception.Message -match 'help instead') {
        throw 'Raw CLI help was improperly shown.'
    }
    $blocked = $true
}
if (-not $blocked) { throw 'Invalid JSON should fail safely.' }
Write-Host '[OK] CLI array args, JSON parse, and safe non-JSON rejection verified.'
