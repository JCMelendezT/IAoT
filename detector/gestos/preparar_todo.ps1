# Baja el dataset publico HaGRID (1 GB), lo prepara y entrena. Un solo comando:
#   powershell -ExecutionPolicy Bypass -File C:\Users\juanc\Desktop\IAoT\detector\gestos\preparar_todo.ps1
# Si se corta la descarga, vuelve a ejecutarlo: continua donde iba.
$ErrorActionPreference = "Stop"
$aqui = $PSScriptRoot
Set-Location $aqui
$dest = Join-Path $aqui "publico"
$zip = Join-Path $dest "hagrid-sample-30k-384p.zip"
$marca = Join-Path $dest "descomprimido.txt"
$url = "https://huggingface.co/datasets/cj-mills/hagrid-sample-30k-384p/resolve/main/hagrid-sample-30k-384p.zip"
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "py" }
New-Item -ItemType Directory -Force $dest | Out-Null

if (-not (Test-Path $marca)) {
    Write-Host "1/4 Bajando el dataset (1 GB, puede tardar)..."
    curl.exe -L -C - --fail --retry 5 -o $zip $url
    if ($LASTEXITCODE -ne 0) { throw "No se pudo bajar el dataset. Revisa el internet y vuelve a ejecutar." }
    Write-Host "2/4 Descomprimiendo (unos minutos)..."
    tar -xf $zip -C $dest
    if ($LASTEXITCODE -ne 0) { throw "No se pudo descomprimir." }
    Set-Content $marca "ok"
    Remove-Item $zip
} else {
    Write-Host "1/4 y 2/4 Ya estaba bajado y descomprimido."
}

Write-Host "3/4 Instalando numpy, pillow y torch (torch pesa unos 200 MB la primera vez)..."
& $py -m pip install --quiet numpy pillow torch
Write-Host "4/4 Preparando las fotos (recorte a 80x60 en gris)..."
& $py (Join-Path $aqui "preparar_publico.py") $dest
if ($LASTEXITCODE -ne 0) { throw "Fallo la preparacion. Copia el mensaje de arriba y pasaselo a Claude." }
Write-Host "Entrenando (unos minutos)..."
& $py (Join-Path $aqui "entrenar_gestos_torch.py")
Write-Host ""
Write-Host "Listo. El modelo quedo en: $aqui\modelo\modelo_gestos.bin"
Write-Host "Subelo en la pagina de la camara (Subir el modelo)."
