# PRUEBA LOCAL v2 (no es para subir). Copia la carpeta Paquete, le cambia SOLO el nombre de identidad
# (ia21.MiauPlayerPrueba) para no chocar con la version de la Store, la empaqueta, la firma e instala.
# Ejecutar en PowerShell como administrador, dentro de la carpeta que contiene "Paquete".
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$src  = Join-Path $root 'Paquete'
$tmp  = Join-Path $env:TEMP 'MiauPlayerPrueba'
$msix = Join-Path $env:TEMP 'MiauPlayer-PRUEBA2.msix'
if (-not (Test-Path (Join-Path $src 'AppxManifest.xml'))) { throw "No encuentro Paquete\AppxManifest.xml en $root" }
if (-not (Test-Path (Join-Path $src 'MiauPlayer.exe')))   { throw "Falta Paquete\MiauPlayer.exe" }

Get-AppxPackage *MiauPlayer* | Remove-AppxPackage -ErrorAction SilentlyContinue
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
Copy-Item $src $tmp -Recurse
$mf = Join-Path $tmp 'AppxManifest.xml'
$x  = Get-Content $mf -Raw -Encoding UTF8
$x  = $x -replace 'Name="ia21\.MiauPlayer"', 'Name="ia21.MiauPlayerPrueba"'
[IO.File]::WriteAllText($mf, $x, (New-Object Text.UTF8Encoding($false)))

$kits = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$makeappx = Get-ChildItem $kits -Recurse -Filter makeappx.exe | Where-Object FullName -like '*\x64\*' | Select-Object -Last 1
$signtool = Get-ChildItem $kits -Recurse -Filter signtool.exe | Where-Object FullName -like '*\x64\*' | Select-Object -Last 1
if (-not $makeappx -or -not $signtool) { throw 'No encuentro makeappx/signtool del SDK de Windows.' }
$makepri = Get-ChildItem $kits -Recurse -Filter makepri.exe | Where-Object FullName -like '*\x64\*' | Select-Object -Last 1
if ($makepri) {   # indice de recursos: sin el, Windows ignora las variantes de icono (targetsize / altform-unplated)
    $cfg = Join-Path $env:TEMP 'miau-priconfig.xml'
    & $makepri.FullName createconfig /cf $cfg /dq es-ES /pv 10.0.0 /o
    if ($LASTEXITCODE -ne 0) { throw 'makepri createconfig fallo' }
    & $makepri.FullName new /pr $tmp /cf $cfg /mn (Join-Path $tmp 'AppxManifest.xml') /of (Join-Path $tmp 'resources.pri') /o
    if ($LASTEXITCODE -ne 0) { throw 'makepri new fallo' }
} else { Write-Host 'AVISO: no encuentro makepri.exe; se empaqueta sin resources.pri' }
if (Test-Path $msix) { Remove-Item $msix -Force }
& $makeappx.FullName pack /d $tmp /p $msix /o
if ($LASTEXITCODE -ne 0) { throw 'makeappx fallo' }

$subject = 'CN=99EC47F4-46C8-451A-8D0C-554779C4B800'
$cert = New-SelfSignedCertificate -Type Custom -Subject $subject -KeyUsage DigitalSignature -FriendlyName 'MiauPlayer prueba 2' `
        -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
$cer = Join-Path $env:TEMP 'MiauPlayerPrueba2.cer'
Export-Certificate -Cert $cert -FilePath $cer | Out-Null
Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
& $signtool.FullName sign /fd SHA256 /sha1 $cert.Thumbprint $msix
if ($LASTEXITCODE -ne 0) { throw 'signtool fallo' }
Add-AppxPackage $msix
Write-Host 'Instalado como "MiauPlayerPrueba". Para quitarlo: Get-AppxPackage ia21.MiauPlayerPrueba | Remove-AppxPackage'
