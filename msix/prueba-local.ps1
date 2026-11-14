# PRUEBA LOCAL (no es para subir al Store): firma una COPIA del paquete con un certificado de prueba e instala MiauPlayer.
# Ejecutar en PowerShell como administrador, en esta carpeta, despues de empaquetar.bat.
$ErrorActionPreference = 'Stop'
$subject = 'CN=99EC47F4-46C8-451A-8D0C-554779C4B800'          # debe coincidir con el publicador del manifiesto
$msix    = Join-Path $PSScriptRoot 'MiauPlayer-1.7.9.0-x64.msix'
$test    = Join-Path $PSScriptRoot 'MiauPlayer-PRUEBA-firmado.msix'
$cert = New-SelfSignedCertificate -Type Custom -Subject $subject -KeyUsage DigitalSignature -FriendlyName 'MiauPlayer prueba' `
        -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
$cer = Join-Path $env:TEMP 'MiauPlayerPrueba.cer'
Export-Certificate -Cert $cert -FilePath $cer | Out-Null
Import-Certificate -FilePath $cer -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
Copy-Item $msix $test -Force
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe | Where-Object FullName -like '*\x64\*' | Select-Object -Last 1
& $signtool.FullName sign /fd SHA256 /sha1 $cert.Thumbprint $test
Add-AppxPackage $test
Write-Host 'Instalado. Busca "MiauPlayer" en el menu Inicio. Para quitarlo: Get-AppxPackage ia21.MiauPlayer | Remove-AppxPackage'
