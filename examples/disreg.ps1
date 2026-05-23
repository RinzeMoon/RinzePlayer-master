# Disable-OBSVulkanLayer.ps1
Write-Host "Disabling OBS Vulkan Layer..." -ForegroundColor Cyan

$path = "HKLM:\SOFTWARE\Khronos\Vulkan\ImplicitLayers"

if (-not (Test-Path $path)) {
    Write-Host "Registry path not found. Nothing to do." -ForegroundColor Yellow
    exit 0
}

$obsKeys = Get-ChildItem $path | Where-Object { $_.PSChildName -like "*obs*" }

if ($obsKeys.Count -eq 0) {
    Write-Host "No OBS Vulkan layer found." -ForegroundColor Yellow
    exit 0
}

foreach ($key in $obsKeys) {
    $oldName = $key.PSChildName
    $newName = $oldName + ".disabled"
    Write-Host "Disabling: $oldName"
    # Rename the key (rename-item)
    Rename-Item -Path $key.PSPath -NewName $newName -ErrorAction SilentlyContinue
    if ($?) {
        Write-Host "  -> Disabled successfully." -ForegroundColor Green
    } else {
        Write-Host "  -> Rename failed, setting value to 0..." -ForegroundColor Yellow
        Set-ItemProperty -Path $path -Name $oldName -Value 0 -Type DWord -Force
    }
}

Write-Host "Done. Please restart your player."